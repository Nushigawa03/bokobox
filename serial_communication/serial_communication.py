import os
import serial
import time
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import keyboard
import configparser

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
# 設定ファイルのパスを取得
config_path = os.path.join(os.path.dirname(__file__), 'config.ini')

# 設定ファイルからcalibration_factorsを読み込む
config = configparser.ConfigParser()
config.read(config_path)

try:
    calibration_factors = list(map(float, config['Calibration']['factors'].split(',')))
except KeyError as e:
    print(f"設定ファイルの読み込みに失敗しました: {e}")
    calibration_factors = [20.0, 50.0, 10.0, 15.0]  # デフォルト値

# データの送信
def send_data(data):
    ser.write(data.encode())

# データの受信と変換
def receive_data():
    global stop_thread
    while not stop_thread:
        if current_mode == "graph_update":
            receive_data_graph_update_mode()
        else:
            receive_data_calibration_mode()

current_mode = "graph_update"  # 初期モードをグラフ更新モードに設定

def on_key_event(event):
    if event.name == 'c':
        if current_mode != "calibration": enter_calibration_mode()
    elif event.name == 'g':
        if current_mode != "graph_update": enter_graph_update_mode()
# キーボード入力イベントを設定
keyboard.on_press(on_key_event)

def enter_calibration_mode():
    global current_mode
    global current_channel
    global current_sample
    current_mode = "calibration"
    current_channel = 0
    current_sample = 0
    print("キャリブレーションモードに入りました")

def enter_graph_update_mode():
    global current_mode
    current_mode = "graph_update"
    print("グラフ更新モードに入りました")


current_channel = 0
current_sample = 0
num_samples = 5
num_channels = 4
samples = np.zeros((num_channels, num_samples))
def receive_data_calibration_mode():
    
    global current_channel
    global current_sample
    global calibration_factors
    try:
        if ser.in_waiting > 0:
            data = ser.readline().decode('utf-8').strip()
            values = [float(x) for x in data.split()]
            samples[current_channel, current_sample] = values[current_channel]
            print(f"Channel {current_channel+1}, Sample {current_sample+1}: {values[current_channel]}")
            current_sample += 1
            if current_sample >= num_samples:
                current_sample = 0
                current_channel += 1
                if current_channel >= num_channels:
                    calibration_factors = np.mean(samples, axis=1).tolist()
                    print(f"Updated calibration factors: {calibration_factors}")
                    # 設定ファイルを更新
                    config['Calibration']['factors'] = ','.join(map(str, calibration_factors))
                    with open(config_path, 'w') as configfile:
                        config.write(configfile)
                    print("設定ファイルを更新しました")
                    enter_graph_update_mode()
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        return -1
    except ValueError:
        print(data)

def receive_data_graph_update_mode():
    global latest_values
    global data_received
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
        return -1
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
        plt.show()
    except KeyboardInterrupt:
        print("Program interrupted")
    finally:
        stop_thread = True
        ser.close()
        serial_thread.join()


if __name__ == "__main__":
    main()