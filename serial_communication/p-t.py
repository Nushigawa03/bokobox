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
                print(data)
                values = [float(x) for x in data.split()]
                latest_values = values
                update_data_history(values)
                means, std_devs = calculate_mean_std(data_history)
                # print_mean_std(means, std_devs)
                data_received = True
        except serial.SerialException as e:
            print(f"Serial error: {e}")
            break
        except ValueError:
            print(data)


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


LEN = 128  # または 1024 に変更
def update_p_t_graph(ax, data):
    time = [i * LEN for i in range(len(data))]
    
    ax.clear()
    ax.plot(time, [d[0] for d in data], label='ch1')
    ax.plot(time, [d[1] for d in data], label='ch2')
    ax.plot(time, [d[2] for d in data], label='ch3')
    ax.plot(time, [d[3] for d in data], label='ch4')
    ax.set(xlabel='Time', ylabel='Values', title='P-T Graph')
    ax.legend()


# メイン関数
def main():
    global stop_thread
    fig, ax = plt.subplots()
    
    ax.set(xlabel='Time', ylabel='Values', title='P-T Graph')
    ax.legend()

    # シリアル通信のスレッドを開始
    serial_thread = threading.Thread(target=receive_data)
    serial_thread.daemon = True
    serial_thread.start()

    def update(frame):
        global data_received
        if not data_received:
            return
        
        if latest_values:
            update_p_t_graph(ax, data_history)
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