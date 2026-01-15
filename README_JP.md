# FrequencyGate

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

ボイスストリーミング向けに最適化された周波数選択型ノイズゲート VST3 プラグイン

[English README](README.md)

## 概要

FrequencyGateは、指定した周波数帯域のみを監視してゲートの開閉を判定し、全周波数帯域にゲートを適用するノイズゲートです。人間の声のエネルギーは特定の周波数帯域（基本周波数として通常100Hz〜500Hz）に集中しているため、従来のゲートよりも効率的に動作します。

### 主な特徴

- **周波数選択型検出**: 指定した周波数範囲のみを監視してゲートのトリガーを判定
- **複数の検出アルゴリズム**: Average（平均）、Peak（ピーク）、Median（中央値）、RMS、Trimmed Mean（刈り込み平均）
- **ヒステリシス**: 開く閾値と閉じる閾値を分離してチャタリングを防止
- **可変FFTサイズ**: 周波数分解能と遅延のトレードオフを調整可能
- **Pre-Open（ルックアヘッド）**: 音声が来る前にゲートを開いてアタックの切れを防止

---

## 必要環境・依存関係

### 必須ソフトウェア

ビルド前に以下をインストールしてください：

| ソフトウェア | バージョン | ダウンロード |
|-------------|-----------|-------------|
| **Visual Studio** | 2019 または 2022 | [ダウンロード](https://visualstudio.microsoft.com/ja/downloads/) |
| **CMake** | 3.15 以上 | [ダウンロード](https://cmake.org/download/) |
| **Git** | 最新版推奨 | [ダウンロード](https://git-scm.com/downloads) |

### Visual Studio インストール時の注意

Visual Studioをインストールする際、以下を選択してください：
- **「C++によるデスクトップ開発」** ワークロード
- または、IDEが不要な場合は **「Build Tools for Visual Studio」** のみでも可

### インストール確認

PowerShellを開いて以下を実行：

```powershell
# CMakeの確認
cmake --version

# Gitの確認
git --version

# Visual Studioの確認（パスが表示されればOK）
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
```

### 自動ダウンロードされる依存関係

ビルドスクリプトが以下を自動的にダウンロードします：
- **DPF** (DISTRHO Plugin Framework) - プラグインフレームワーク
- **PFFFT** (Pretty Fast FFT) - FFTライブラリ

これらを手動でインストールする必要は**ありません**。

---

## ビルド方法

### クイックスタート（Windows）

```powershell
# リポジトリをクローン
git clone https://github.com/yourusername/FrequencyGate.git
cd FrequencyGate

# ビルド
.\build.ps1

# ビルドしてシステムのVST3フォルダにインストール
.\build.ps1 -Install
```

### ビルドオプション

```powershell
# クリーンビルド（生成ファイルを削除して依存関係を再ダウンロード）
.\build.ps1 -Clean

# リリースビルド（最適化、デバッグ情報なし）
.\build.ps1 -Release

# リリースビルド＆インストール
.\build.ps1 -Release -Install
```

### ビルド成果物

ビルド成功後、VST3プラグインは以下に生成されます：
```
build\bin\FrequencyGate.vst3
```

### インストール

#### 自動インストール
```powershell
.\build.ps1 -Install
```

#### 手動インストール
`FrequencyGate.vst3` フォルダを以下にコピー：
- **Windows**: `C:\Program Files\Common Files\VST3\`

---

## 使用方法

### プラグインの読み込み

DAWまたはストリーミングソフトウェアでVST3プラグインとしてFrequencyGateを読み込みます：
- **OBS Studio**: VST 2.x/3.x プラグインフィルタとして追加
- **Reaper**: トラックにFXとして挿入
- **その他のDAW**: 標準的なVST3プラグイン読み込み

### パラメータ

#### 周波数範囲
| パラメータ | 範囲 | デフォルト | 説明 |
|-----------|------|-----------|------|
| **Freq Low** | 20 Hz - 20 kHz | 100 Hz | 検出範囲の下限周波数 |
| **Freq High** | 20 Hz - 20 kHz | 500 Hz | 検出範囲の上限周波数 |

#### 閾値設定
| パラメータ | 範囲 | デフォルト | 説明 |
|-----------|------|-----------|------|
| **Threshold** | -96 dB 〜 0 dB | -30 dB | ゲートが開く閾値 |
| **Hysteresis** | 0 dB 〜 12 dB | 3 dB | 開く閾値と閉じる閾値の差 |
| **Range** | -96 dB 〜 0 dB | -96 dB | ゲートが閉じた時の減衰量 |

#### 検出方法
| 方法 | 説明 | 最適な用途 |
|------|------|-----------|
| **Average** | 範囲内の全マグニチュードの平均 | 一般的な音声（デフォルト） |
| **Peak** | 範囲内の最大マグニチュード | トランジェント重視 |
| **Median** | 外れ値を無視する中央値 | ノイズの多い環境 |
| **RMS** | 二乗平均平方根（エネルギーベース） | 安定したレベル |
| **Trimmed Mean** | 上下10%を除外した平均 | ノイズ除去に最適 |

#### エンベロープ
| パラメータ | 範囲 | デフォルト | 説明 |
|-----------|------|-----------|------|
| **Pre-Open** | 0 ms - 20 ms | 0 ms | ルックアヘッド時間（遅延が追加される） |
| **Attack** | 0.1 ms - 100 ms | 5 ms | ゲートが完全に開くまでの時間 |
| **Hold** | 0 ms - 500 ms | 50 ms | 信号が下がった後もゲートを開いたままにする時間 |
| **Release** | 1 ms - 1000 ms | 100 ms | ゲートが完全に閉じるまでの時間 |

#### FFTサイズ
| オプション | 遅延（概算） | 周波数分解能 |
|-----------|-------------|-------------|
| 512 | 約5 ms | 低 |
| 1024 | 約10 ms | 中 |
| **2048** | 約21 ms | 高（推奨） |
| 4096 | 約42 ms | 非常に高 |

### ボイスストリーミング向け推奨設定

```
Freq Low:         100 Hz
Freq High:        500 Hz
Threshold:        -30 dB  （マイク/環境に応じて調整）
Detection:        Average または Trimmed Mean
Hysteresis:       3 dB
Pre-Open:         0 ms
Attack:           5 ms
Hold:             50 ms
Release:          100 ms
Range:            -96 dB
FFT Size:         2048
```

---

## 動作原理

### 従来のゲート vs FrequencyGate

**従来のゲート：**
- 全周波数帯域を監視
- 全周波数にわたる環境ノイズが誤動作を引き起こす可能性
- キーボードの打鍵音、エアコンのハム音など、声以外の音で簡単にゲートが開く

**FrequencyGate：**
- 声の基本周波数が存在する周波数範囲のみを監視（例：100-500Hz）
- 高周波ノイズ（キーボード、マウスクリック）はトリガー判定で無視
- 検出範囲外の低周波のゴロゴロ音はゲート判定に影響しない
- 結果：誤動作が少なく、より信頼性の高い音声検出

### 信号フロー

```
入力 → FFT解析 → 周波数帯域検出 → ゲート判定 → ゲインエンベロープ → 出力
           ↓
   [指定周波数範囲のみを解析]
           ↓
   [全帯域にゲートを適用]
```

---

## トラブルシューティング

### UIに文字が表示されない
プラグインはシステムフォントの読み込みを試みます。フォントの読み込みに失敗した場合、数値は7セグメント風のフォールバック表示になります。これは見た目の問題であり、機能には影響しません。

### CPU使用率が高い
- FFTサイズを小さくする（小さい＝CPU負荷低、ただし周波数分解能も低下）
- プラグインはリアルタイム使用に最適化されており、通常はCPU負荷は最小限

### ゲートが正しく動作しない
1. 声の周波数範囲がFreq Low/Highの設定内にあるか確認
2. "Average"で反応が鈍い場合は"Peak"検出方法を試す
3. ゲートが開かない場合はThresholdを下げる
4. ゲートがチャタリングする場合はHysteresisを上げる

---

## 技術情報

- **フレームワーク**: DPF (DISTRHO Plugin Framework)
- **FFTライブラリ**: PFFFT (Pretty Fast FFT)
- **UI**: NanoVGベースのカスタムUI
- **フォーマット**: VST3
- **対応OS**: Windows（主要）、Linux/macOS（セカンダリ）

---

## ライセンス

このプロジェクトはMITライセンスの下で公開されています。詳細は[LICENSE](LICENSE)ファイルをご覧ください。

## サードパーティライブラリ

- [DPF](https://github.com/DISTRHO/DPF) - ISC License
- [PFFFT](https://bitbucket.org/jpommier/pffft/) - BSD-like License

## コントリビューション

コントリビューションを歓迎します！Issueやプルリクエストをお気軽にお送りください。

## 謝辞

- 優れたDPFフレームワークを提供してくれたDISTROHOチーム
- PFFTを作成したJulien Pommier氏
