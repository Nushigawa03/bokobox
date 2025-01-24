import serial
import time
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation

import numpy as np


# シリアルポートの設定
ser = serial.Serial(
    port='COM3',  # 使用するポート名に変更してください
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=1
)

# グローバル変数
latest_values = None
data_received = False
stop_thread = False
microphone_positions = [
    (0.5, 0), (0, 0.5), (-0.5, 0), (0, -0.5)
]
reference_positions = [
    (0, 0), (0.5, 0), (0.5, 0.5), (0, 0.5), (-0.5, 0.5), (-0.5, 0), (-0.5, -0.5), (0, -0.5), (0.5, -0.5)
]
calibration_factors = [20.0, 50.0, 10.0, 15.0]  # LEN=256
calibration_factors = [50.0, 80.0, 50.0, 20.0]  # LEN=1024
# positive_calibration_factors = [50.0, 80.0, 50.0, 20.0]  # LEN=1024
# negative_calibration_factors = [20.0, 40.0, 50.0, 10.0]  # LEN=1024

# データの送信
def send_data(data):
    ser.write(data.encode())

# データの受信と変換
def receive_data():
    global latest_values
    global data_received
    global stop_thread
    while not stop_thread:
        try:
            if ser.in_waiting > 0:
                data = ser.readline().decode('utf-8').strip()
                # print(data)
                values = [float(x) for x in data.split()]
                latest_values = values
                update_data_history(values)
                means, std_devs = calculate_mean_std(data_history)
                print_mean_std(means, std_devs)
                data_received = True
        except serial.SerialException as e:
            print(f"Serial error: {e}")
            break
        except ValueError:
            print(data)
def normalize_radius(radius, positive_calibration, negative_calibration):
    if positive_calibration == negative_calibration:
        raise ValueError("Positive and negative calibration factors must be different.")
    
    normalized_value = (radius - negative_calibration) / (positive_calibration - negative_calibration)
    return max(0, min(1, normalized_value))

# 円の大きさを更新
def update_circles(frame, ax, circles):
    global latest_values
    global calibration_factors
    if latest_values and len(latest_values) >= 4:
        for i, circle in enumerate(circles):
            radius = latest_values[i] / calibration_factors[i] / 10  # 適宜スケーリング
            circle.set_radius(radius)
        ax.figure.canvas.draw()


# 直前10回のデータを保持するリスト
data_history = []

def update_data_history(new_values):
    global data_history
    data_history.append(new_values)
    if len(data_history) > 10:
        data_history.pop(0)

def calculate_mean_std(data_history):
    data_array = np.array(data_history)
    means = np.mean(data_array, axis=0)
    std_devs = np.std(data_array, axis=0)
    return means, std_devs

def print_mean_std(means, std_devs):
    for mean, std_dev in zip(means, std_devs):
        print(f"{mean:.6f} ± {std_dev:.6f}", end=' ' * 5)
    print()

def plot_reference_positions(ax, reference_positions):
    for i, (x, y) in enumerate(reference_positions):
        ax.plot(x, y, 'ro', markersize=1) 
        ax.text(x, y, f'P{i}', fontsize=12, ha='right')  # テキストラベルを追加

def update_pie_chart(ax, values):
    global calibration_factors
    scaled_values = [value / calibration_factors[i] for i, value in enumerate(values)]
    ax.clear()
    ax.pie(scaled_values, labels=[f"Channel {i+1}" for i in range(len(scaled_values))], autopct='%1.1f%%')
    ax.figure.canvas.draw()

    # 重心を計算
    centroid = calculate_centroid(scaled_values)
    # 最も近い基準位置に正規化
    normalized_position = normalize_to_closest_reference(centroid)

    return centroid

def calculate_centroid(values):
    global microphone_positions
    total = sum(values)
    weighted_positions = [(x * value, y * value) for (x, y), value in zip(microphone_positions, values)]
    centroid_x = sum(x for x, y in weighted_positions) / total
    centroid_y = sum(y for x, y in weighted_positions) / total
    return (centroid_x, centroid_y)

def get_centroid_std(centroid_history):
    x_values = [point[0] for point in centroid_history]
    y_values = [point[1] for point in centroid_history]
    return np.std(x_values), np.std(y_values)

def print_centroid_mean_std(centroid_history):
    x_values = [point[0] for point in centroid_history]
    y_values = [point[1] for point in centroid_history]
    mean_x, mean_y = np.mean(x_values), np.mean(y_values)
    std_x, std_y = np.std(x_values), np.std(y_values)
    print(f"重心: ({mean_x:.6f} ± {std_x:.6f}, {mean_y:.6f} ± {std_y:.6f})")

def normalize_to_closest_reference(centroid):
    distances = [np.linalg.norm(np.array(centroid) - np.array(pos)) for pos in reference_positions]
    min_distance_index = np.argmin(distances)
    return reference_positions[min_distance_index]

# メイン関数
def main():
    global stop_thread
    global microphone_positions
    fig, (ax1, ax2) = plt.subplots(1, 2)
    circles = [plt.Circle(pos, 0, color='blue') for pos in microphone_positions]
    for circle in circles:
        ax1.add_patch(circle)
    ax1.set_xlim(-0.7, 0.7)
    ax1.set_ylim(-0.7, 0.7)
    ax1.set_aspect('equal', adjustable='box')
    ax2.pie([1], colors=['gray'])
    plot_reference_positions(ax1, reference_positions)

    # シリアル通信のスレッドを開始
    serial_thread = threading.Thread(target=receive_data)
    serial_thread.daemon = True
    serial_thread.start()

    # 重心の履歴を保持するリスト
    centroid_history = []
    plotted_points = []
    colors = ['#FF0000', '#FF4000', '#FF8000', '#FFBF00', '#FFFF00', '#BFFF00', '#80FF00', '#40FF00', '#00FF00', '#00FF40']

    def update(frame):
        global data_received
        if not data_received:
            return
        
        update_circles(frame, ax1, circles)
        if latest_values:
            centroid = update_pie_chart(ax2, latest_values)
            centroid = (centroid[0], centroid[1])
            # 新しい点をプロット
            point, = ax1.plot(centroid[0], centroid[1], 'o', color=colors[0])
            centroid_history.append(centroid)
            plotted_points.append(point)
                    
            # リストが10を超えた場合、最も古い点を削除
            if len(plotted_points) > 10:
                old_point = plotted_points.pop(0)
                old_point.remove()
                centroid_history.pop(0)
            # プロットされた点の色を更新
            for i, point in enumerate(plotted_points):
                point.set_color(colors[max(0, min(len(plotted_points), len(colors)) - 1 - i)])
            print_centroid_mean_std(centroid_history)
        data_received = False
            
    ani = animation.FuncAnimation(fig, update, interval=100, cache_frame_data=False)

    try:
        while True:
            mode = input("Enter mode (c): ").strip().lower()
            if mode == 'c':
                calibration_mode()
            else:
                print("Invalid mode. Please enter 'a', 'b', 'calibrate', or 'exit'.")
        plt.show()
    except KeyboardInterrupt:
        print("Program interrupted")
    finally:
        stop_thread = True
        ser.close()
        serial_thread.join()


def calibration_mode():
    global calibration_factors
    directions = ['left', 'up', 'right', 'down']
    for idx, direction in enumerate(directions):
        print(f"Calibrating {direction}. Please hit the {direction} button 5 times.")
        values = []
        for i in range(5):
            value = float(input(f"Enter value {i+1} for {direction}: "))
            values.append(value)
        mean, _ = calculate_mean_std(values)
        calibration_factors[idx] = mean
    print("Calibration complete. Updated calibration factors:")
    print(calibration_factors)

if __name__ == "__main__":
    main()