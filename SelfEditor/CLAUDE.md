# CLAUDE.md

このファイルは、リポジトリ内のコードを扱う Claude Code (claude.ai/code) へのガイダンスを提供します。

## プロジェクト概要

**SelfEditor** は DirectX ベースのリズムアクションゲーム向け Timeline Event Editor です。一般的な音ゲー譜面エディタではなく、**音楽同期イベントを DAW 風タイムラインに配置する Timeline Event Editor** として設計されています。

対象ゲームは固定スクロール型リズムアクション（オブジェクトが奥から手前に流れてくる形式）で、重力切り替えギミックを持ちます。このエディタはステージイベントを管理します。

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
- ノーツ種類を Enemy/Hold/Orb/Barrier に刷新・形状で視覚的区別
- オフセット機能（`Offset(s)` で再生ヘッドをずらす）
- レーン順変更（Up/Left/Down/Right）
- ショートカットキー（Space=再生/一時停止トグル、Enter=停止＆先頭へ）
- スロー再生（0.25x/0.5x/0.75x/1.0x、Speed コンボ UI）
- 波形表示（タイムライン背景にシアン重畳・ズーム連動）
- ノーツSE再生（再生中にビート通過時に発火、`Assets/SE/` から WAV/MP3 読込）
- SePlayer（ポリフォニック 4 ボイスプール・SE 別音量制御）
- AudioLoader（WAV/MP3 共通ローダー、`Core/Audio/AudioLoader.h/.cpp`）
- シーク位置マーカー（M=再生ヘッド位置をマーカーへ記録、Shift+M=カーソル位置にマーカーを移動、->M ボタン=マーカー位置へシーク＆ビュースクロール、三角形クリックでシーク）
- コピー&ペースト&切り取り&削除（Ctrl+ドラッグで範囲選択・青ハイライト表示、Ctrl+C でコピー、Ctrl+V でカーソル位置にペースト、Ctrl+X で切り取り、Delete で選択範囲を削除、Hold/Rainbow は範囲内に完全に収まるものだけ対象）
- ヘッダー行（小節番号エリア）クリックで再生ヘッドをその beat 位置にシーク
- Rainbowノーツ追加（壁全体・壁またぎ可・2クリック配置・同一壁は虹色グラデーション・壁またぎはシアン緑単色）
- Holdノーツ刷新（同一壁・レーンまたぎ可・2クリック配置・同一レーンはバー・異レーンは平行四辺形）
- JSONメタデータ追加（musicname/musicauthor/scoreauthor/difficulty/thumbnail）
- エディタ UI 3段構成化（Row1: メタデータ、Row2: ファイル/BPM/スナップ/ズーム/タイプ/オフセット、Row3: 音楽/トランスポート）
- JSON 保存キー順序の修正（`nlohmann::ordered_json` 使用、挿入順を保持）
- 日本語フォント対応（`C:\Windows\Fonts\meiryo.ttc` + `GetGlyphRangesJapanese()` で全角入力サポート）
- Offset値のJSON保存/読込対応（`Chart::offset` フィールド追加、Save/Open時に `m_offsetSec` と同期）
- File/Thumbnail/Music にネイティブファイル選択ダイアログ（`...` ボタン、Win32 `GetOpenFileNameA`）を追加。File欄は選択した絶対パスをそのまま使用、Thumbnail/Musicはファイル名のみ抽出して既存の相対パス規約を維持
- 音楽フォルダ移動（`Assets/music/` → ルート直下 `music/`）に伴うパス参照修正、譜面JSONの既定保存先を `json/` フォルダに変更

### 作業未着手
（なし）

## 技術構成

- **言語**: C++
- **ビルド**: Visual Studio 2022 (v145 ツールセット)、ソリューションは `SelfEditor/SelfEditor.sln`
- **構成**: Debug/Release × Win32/x64
- **ライブラリ**: DirectX、Dear ImGui、XAudio2、minimp3、nlohmann/json

## ビルド方法

Visual Studio 2022 で `SelfEditor/SelfEditor.sln` を開き、IDE からビルド (Ctrl+Shift+B)。またはコマンドラインから MSBuild を使用:

```
msbuild SelfEditor/SelfEditor.sln /p:Configuration=Debug /p:Platform=x64
```

Release ビルドの実行には exe と同ディレクトリに `Assets/`・`music/`・`json/` フォルダを配置すること。

## 推奨ディレクトリ構造

ソースファイルを追加する際は以下のレイアウトに従う:

```
SelfEditor/
├ Core/
│   ├ Chart/       # 譜面データ構造
│   ├ Audio/       # 音声再生（AudioLoader / AudioPlayer / SePlayer）
│   ├ Timing/      # BPM・beat→時間変換
│   └ Event/       # イベント定義
├ Game/            # ゲーム側の簡易イベント再生
├ Editor/          # ImGui ベースのエディタ UI
├ Assets/          # SE・エディタ設定
│   └ SE/
├ music/           # 音楽ファイル（ルート直下）
└ json/            # 譜面 JSON ファイル（ルート直下、既定の保存先）
```

`Core/` は `Game/` と `Editor/` の両方から共有される。

## コアデータ設計

### ノーツ種類
| 種類 | 形状 | 説明 |
|------|------|------|
| Enemy   | 塗り円 ● | 通常ノーツ（JSON 文字列: `"Tap"`）|
| Hold    | バー or 平行四辺形 | 同一壁・レーンまたぎ可・壁またぎ不可。同一レーンはバー、異レーンは平行四辺形 |
| Orb     | 中抜き円 ○ | |
| Barrier | 四角 ■ | |
| Rainbow | 平行四辺形（虹色） | 壁全体（lane=1 固定）・壁またぎ可・2クリック配置 |

### beat ベースのタイミング管理（重要）

イベント位置は必ず **beat** で管理し、秒数は使わない。秒への変換は実行時に行う:

```
time = (60.0f / BPM) * beat
```

BPM が変化すると秒ベースのタイムスタンプは無効になるため、beat が正規の単位となる。

### イベント構造体

```cpp
enum class EventType { Enemy, Hold, Orb, Barrier, Rainbow };
enum class Wall      { Up, Left, Down, Right };  // 表示順 = enum値

struct Event {
    float     beat    = 0.0f;
    float     endBeat = 0.0f;  // Hold / Rainbow のみ使用
    EventType type    = EventType::Enemy;
    Wall      wall    = Wall::Up;
    Wall      endWall = Wall::Up;  // Rainbow のみ使用（壁またぎ対応）
    int       lane    = 0;         // 0-2、Rainbow は常に 1（中央）
    int       endLane = 0;         // Hold のみ使用（レーンまたぎ対応）
};
```

ステージは 4 壁（Up/Left/Down/Right）× 3 レーン = 12 レーン。

### 譜面 JSON フォーマット

```json
{
  "musicname": "シャイニングスター",
  "musicauthor": "森田交一",
  "scoreauthor": "R.T",
  "difficulty": 5.0,
  "thumbnail": "shiningstar.png",
  "bpm": 160,
  "music": "stage01.mp3",
  "events": [
    { "beat": 4.0,  "type": "Tap",     "wall": "Up",    "lane": 1 },
    { "beat": 8.0,  "type": "Hold",    "wall": "Left",  "lane": 0, "endBeat": 10.0, "endLane": 2 },
    { "beat": 12.0, "type": "Orb",     "wall": "Down",  "lane": 0 },
    { "beat": 16.0, "type": "Barrier", "wall": "Right", "lane": 2 },
    { "beat": 20.0, "type": "Rainbow", "wall": "Up",    "lane": 1, "endBeat": 24.0, "endWall": "Down" }
  ]
}
```

キー順序は `nlohmann::ordered_json` により挿入順を保持。
音楽ファイルは `music/` に配置し、Music 欄にはファイル名のみ入力（`music/` は自動付加）。
譜面 JSON ファイルは `json/` に配置し、`File` 欄にはファイル名のみ入力（`json/` は自動付加、既定値: `.json`）。

## エディタ操作

| 操作 | 動作 |
|------|------|
| 左クリック | ノーツ配置 / トグル削除 |
| 右クリック | ノーツ削除 |
| マウスホイール | タイムラインスクロール |
| Space | 再生 / 一時停止トグル |
| Enter | 停止して先頭へ戻る |
| M | 再生ヘッド位置をシークマーカーに記録 |
| Shift+M | カーソル位置にシークマーカーを移動 |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+ドラッグ | 範囲選択 |
| Ctrl+C / Ctrl+V | 選択範囲コピー / カーソル位置へペースト |
| Ctrl+X | 選択範囲を切り取り（コピー＋削除） |
| Delete | 選択範囲のノーツを削除 |
| Esc | Hold / Rainbow pending・選択キャンセル |

Hold 配置: 1回目クリックで始点、2回目クリック（**同一壁**のみ）で終点確定。異なるレーンをクリックするとレーンまたぎになる。
Rainbow 配置: 1回目クリックで始点、2回目クリック（任意の壁）で終点確定。

## 開発フェーズ

| フェーズ | 内容 |
|---------|------|
| 1 | Core: Event 構造体・JSON 保存/読込・BPM→時間変換 |
| 2 | Game: JSON ロード・イベント流し・BPM 同期確認 |
| 3 | Editor: ImGui Timeline UI・レーン表示・ノーツ配置・保存/読込 |
| 4 | 音楽同期: 音楽再生・再生位置同期・再生ヘッド |
| 5 | 拡張: ノーツ刷新・Hold・オフセット・レーン順変更 |
| 6 | 拡張: 波形表示・SE再生・スロー再生・シークマーカー |
| 7 | 拡張: Rainbow追加・Hold刷新・メタデータ・日本語対応 |
