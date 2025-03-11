# Bokobox

[**ProtoPedia記事**](./)
## 概要
Bokoboxは、Arduinoを使用してマイクの入力を位置とパワーに変換するプロジェクト。メインコアとサブコアを使用して、オーディオデータの収集と解析を行う。また、Pythonを使用してデータの可視化やシリアル通信を行う。

## システム構成
- **MainAudio**: MainCore 768KB
- **SubAudio**: SubCore1 256KB

## 必要なライブラリ
以下のPythonライブラリが必要となる。

- matplotlib
- pyserial
- keyboard

```
pip install matplotlib
pip install pyserial
pip install keyboard
```

## 使用方法
1. Arduino IDEを使用して、SpresenseにMainAudioとSubAudioを書き込む。

1. シリアル通信スクリプトを実行することで、アプリが起動される。

```
python serial_communication/serial_communication.py
```

## ディレクトリ構成
```
bokobox
├── Arduino
│   ├── MainAudio
│   └── SubAudio
├───serial_communication
└───resources
    ├── 3d_models
    └── docs
```

### ディレクトリ説明
- **Arduino**: Arduinoプロジェクトのコードが含まれている。
  - **MainAudio**: メインコア用のオーディオ処理コード。
  - **SubAudio**: サブコア用のオーディオ処理コード。
- **serial_communication**: シリアル通信を行うPythonスクリプトが含まれている。
- **resources**: プロジェクトに関連するリソースが含まれている。
  - **3d_models**: 3Dモデルファイル。
  - **docs**: プロジェクトの検証レポート。
