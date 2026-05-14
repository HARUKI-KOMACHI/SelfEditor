# CLAUDE.md

このファイルは、リポジトリ内のコードを扱う Claude Code (claude.ai/code) へのガイダンスを提供します。

## プロジェクト概要

**SelfEditor** は DirectX ベースのリズムアクションゲーム向け Timeline Event Editor です。一般的な音ゲー譜面エディタではなく、**音楽同期イベントを DAW 風タイムラインに配置する Timeline Event Editor** として設計されています。

対象ゲームは固定スクロール型リズムアクション（オブジェクトが奥から手前に流れてくる形式）で、重力切り替えギミックを持ちます。このエディタは敵・障害物・重力変更などのステージイベントを管理します。

## ワークフロー

`\update` と入力すると CLAUDE.md を現在の実装状況に合わせて更新する。

実装作業を始める前に必ず Plan モード (`/plan`) に入り、変更内容を確認してから実装すること。

ユーザーの入力が `？` で終わる場合は、プロジェクトに一切手を加えず質問への回答のみ行うこと。

## 作業状況

### 作業完了
- Phase 1: Event 構造体・JSON 保存/読込・BPM→時間変換
- Phase 2: EventStream（ゲーム側イベント再生シミュレーション）
- Phase 3: ImGui Timeline UI・レーン表示・ノーツ配置/削除・JSON 保存/読込
- Phase 4: XAudio2 音楽再生（WAV・MP3）・再生ヘッド・自動スクロール
- Undo/Redo（Ctrl+Z / Ctrl+Y、最大100ステップ）
- MP3 対応（minimp3 シングルヘッダー、`SelfEditor/minimp3/` に配置）

### 作業未着手
- HOLD ノーツ
- 波形表示

## 技術構成

- **言語**: C++
- **ビルド**: Visual Studio 2022 (v143 ツールセット)、ソリューションは `SelfEditor/SelfEditor.sln`
- **構成**: Debug/Release × Win32/x64
- **ライブラリ**: DirectX、Dear ImGui

## ビルド方法

Visual Studio 2022 で `SelfEditor/SelfEditor.sln` を開き、IDE からビルド (Ctrl+Shift+B)。またはコマンドラインから MSBuild を使用:

```
msbuild SelfEditor/SelfEditor.sln /p:Configuration=Debug /p:Platform=x64
```

## 推奨ディレクトリ構造

ソースファイルを追加する際は以下のレイアウトに従う:

```
SelfEditor/
├ Core/
│   ├ Chart/       # 譜面データ構造
│   ├ Audio/       # 音声再生
│   ├ Timing/      # BPM・beat→時間変換
│   └ Event/       # イベント定義
├ Game/            # ゲーム側の簡易イベント再生
├ Editor/          # ImGui ベースのエディタ UI
└ Assets/          # 音楽ファイル、JSON 譜面ファイル
```

`Core/` は `Game/` と `Editor/` の両方から共有される。

## コアデータ設計

### beat ベースのタイミング管理（重要）

イベント位置は必ず **beat** で管理し、秒数は使わない。秒への変換は実行時に行う:

```
time = (60.0f / BPM) * beat
```

BPM が変化すると秒ベースのタイムスタンプは無効になるため、beat が正規の単位となる。

### イベント構造体

```cpp
enum class EventType        { Enemy, Obstacle, Gravity, Jump };
enum class Wall             { Up, Down, Left, Right };
enum class GravityDirection { Up, Down, Left, Right };

struct Event {
    float            beat       = 0.0f;
    EventType        type       = EventType::Enemy;
    Wall             wall       = Wall::Up;   // Gravity 以外
    int              lane       = 0;          // 0-2
    GravityDirection gravityDir = GravityDirection::Down; // Gravity のみ
};
```

ステージは 4 壁 × 3 レーン = 12 レーン。Gravity イベントは wall/lane を持たず gravityDir のみ。

### 譜面 JSON フォーマット

```json
{
  "music": "stage01.mp3",
  "bpm": 160,
  "events": [
    { "beat": 4.0, "type": "Enemy",   "wall": "Up",   "lane": 1 },
    { "beat": 8.0, "type": "Gravity", "direction": "Left" }
  ]
}
```

音楽ファイルは `Assets/music/` に配置し、Music 欄にはファイル名のみ入力（`Assets/music/` は自動付加）。

## 開発フェーズ

| フェーズ | 内容 |
|---------|------|
| 1 | Core: Event 構造体・JSON 保存/読込・BPM→時間変換 |
| 2 | Game: JSON ロード・イベント流し・BPM 同期確認 |
| 3 | Editor: ImGui Timeline UI・レーン表示・ノーツ配置・保存/読込 |
| 4 | 音楽同期: 音楽再生・再生位置同期・再生ヘッド |
| 5 | 拡張: HOLD ノーツ・波形表示・Undo/Redo・MP3対応 |

**SLIDE ノーツは早期実装しない** — ノード管理・曲線制御・ドラッグ処理など複雑化しやすいため後回しにする。

## エディタ UI

DAW 風のタイムライン UI を目指す:

```
━━━━━━━━━━━━━━━━
|1|2|3|4|1|2|3|4|
━━━━━━━━━━━━━━━━
Lane1 ●       ●
Lane2     ●
Lane3         ●
```

Version 1 必須機能: BPM 設定・小節線表示・レーン表示・ノーツ配置/削除・JSON 保存/読込・音楽再生・再生位置表示。
