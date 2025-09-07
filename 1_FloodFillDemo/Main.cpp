# include <Siv3D.hpp>

// グリッドのセル情報
struct Cell {
	bool wall = false;
	bool visited = false;
	Color color = Palette::White;
};

struct Frame {
	Point p;
	int dir = 0; // 次に試す方向インデックス [0..3] (Dirs参照)
};

enum class Phase { Prep, Explore, Done };

namespace {
	static constexpr int W = 8;
	static constexpr int H = 8;
	constexpr int cellSize = 60;
	const Point gridOrigin(40, 40);

	// 4方向ベクトル（右、下、左、上）
	Array<Point> Dirs = { Point{1,0}, Point{0,1}, Point{-1,0}, Point{0,-1} };

	Rect cellRect(int x, int y) {
		return Rect(gridOrigin.x + x * cellSize, gridOrigin.y + y * cellSize, cellSize, cellSize);
	}

	bool inBounds(const Point& p) {
		return (0 <= p.x && p.x < W && 0 <= p.y && p.y < H);
	}
}

// メイン関数
void Main() {
	Window::Resize(800, 560);
	Scene::SetBackground(ColorF(0.96));

	// グリッドのセル情報
	Array<Array<Cell>> grid(H, Array<Cell>(W));

	// 最初は準備フェーズ
	Phase phase = Phase::Prep;

	const Rect confirmBtn(gridOrigin.x + W * cellSize + 20, gridOrigin.y, 140, 44);

	// 探索アニメーション用
	Stopwatch stepSW{ StartImmediately::Yes };
	double stepInterval = 0.15; // 1 ステップの間隔

	// 未探索の集合の根を走査する用の変数
	int scanIndex = 0;

	// DFS 状態
	bool dfsActive = false;
	Array<Frame> callStack;
	bool justPushed = false;
	Color currentColor = Palette::White;

	const int maxStackLines = 22;
	Font font(16), fontTitle(18, Typeface::Bold);

	// リセット
	auto resetVisit = [&] {
		for (auto& row : grid) for (auto& c : row) {
			c.visited = false;
			c.color = Palette::White;
		}
		scanIndex = 0;
		dfsActive = false;
		callStack.clear();
		justPushed = false;
	};

	// 1 ステップ進める
	auto stepExplore = [&] {
		if (dfsActive) {

			if (callStack.isEmpty()) {
				dfsActive = false;
				return;
			}

			// 直前に push したノードなら、まず「訪問確定」を1ステップで行う
			if (justPushed) {
				const Point p = callStack.back().p;
				grid[p.y][p.x].visited = true;
				grid[p.y][p.x].color = currentColor;
				justPushed = false;
				return;
			}

			// トップのフレームを参照
			Frame& top = callStack.back();

			// 次に試す方向
			while (top.dir < 4) {
				const Point np = top.p + Dirs[top.dir++];
				if (!inBounds(np)) continue;
				if (grid[np.y][np.x].wall) continue;
				if (grid[np.y][np.x].visited) continue;

				// スタックに積む
				callStack << Frame{ np, 0 };
				justPushed = true; // 次ステップで「訪問確定」に専念
				return;
			}

			// 全方向試し終えたら、戻る
			callStack.pop_back();
			return;
		}
		else {
			if (scanIndex >= (W * H)) {
				phase = Phase::Done;
				return;
			}

			const int y = scanIndex / H;
			const int x = scanIndex % H;

			// 未訪問の通常セルを見つけたら、そこから深さ優先探索開始
			if ((!grid[y][x].wall) && (!grid[y][x].visited)) {
				// 深さ優先探索をはじめる
				currentColor = HSV(Random(0.0, 360.0), 0.65, 0.92);
				callStack.clear();
				callStack << Frame{ Point{x, y}, 0 };
				dfsActive = true;
				justPushed = true;
				return;
			}

			// 壁 or 訪問済みなら、ここで 1 マスだけ進めて終了
			++scanIndex;
			return;
		}
	};

	while (System::Update()) {

		// ==== 準備フェーズ ====
		if (phase == Phase::Prep) {
			// クリックで壁設定（左クリックでセット、右クリックで解除）
			if (MouseL.down()) {
				const Point pos = Cursor::Pos();
				for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
					if (cellRect(x, y).mouseOver() && !grid[y][x].wall) {
						grid[y][x].wall = true;
					}
				}
			}
			if (MouseR.down()) {
				const Point pos = Cursor::Pos();
				for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
					if (cellRect(x, y).mouseOver() && grid[y][x].wall) {
						grid[y][x].wall = false;
					}
				}
			}

			// 「確定」ボタン
			const bool over = confirmBtn.mouseOver();
			const Color btnColor = over ? ColorF(0.2, 0.6, 1.0) : ColorF(0.15, 0.5, 0.95);
			confirmBtn.rounded(8).draw(btnColor);
			fontTitle(U"確定").drawAt(confirmBtn.center(), Palette::White);

			if (over && MouseL.down()) {
				phase = Phase::Explore;
				resetVisit();
			}

			fontTitle(U"左クリック：壁を置く / 右クリック：壁を消す").draw(gridOrigin.x, gridOrigin.y - 30, ColorF(0.1));
		}

		// ==== 描画 ====
		// グリッド背景
		for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
			const Rect r = cellRect(x, y);
			if (grid[y][x].wall) {
				r.stretched(-1).draw(ColorF(0.25)); // 壁
			}
			else if (grid[y][x].visited) {
				r.stretched(-1).draw(grid[y][x].color); // 訪問済みを色塗り
			}
			else {
				r.stretched(-1).draw(Palette::White); // 未訪問
			}
			r.drawFrame(1, 0, ColorF(0.75));
		}

		// 未訪問カーソル
		if (phase == Phase::Explore) {
			int cx = 0, cy = 0;
			if (scanIndex < W * H) {
				cy = scanIndex / H;
				cx = scanIndex % H;
			}
			else {
				cx = cy = 0;
			}
			// 強調枠
			cellRect(cx, cy).drawFrame(3, 0, ColorF(1.0, 0.9, 0.2, 0.9));
		}

		// スタックビジュアライザ
		const int panelX = gridOrigin.x + W * cellSize + 20;
		const int panelY = gridOrigin.y + 75;
		const Rect stackPanel(panelX - 10, gridOrigin.y + 50, 250, 430);
		stackPanel.draw(ColorF(1.0)).drawFrame(1, 0, ColorF(0.8));
		fontTitle(U"再帰スタック").draw(panelX, gridOrigin.y + 54, ColorF(0.1));

		// スタック内容
		if (!callStack.isEmpty()) {
			const int total = static_cast<int>(callStack.size());
			const int start = Max(0, total - maxStackLines);
			int line = 0;
			for (int i = start; i < total; ++i) {
				const auto& fr = callStack[i];
				String s = U"[{0}] ({1},{2})"_fmt(i, fr.p.x, fr.p.y);
				font(s).draw(panelX, panelY + line * 18, ColorF(0.15));
				++line;
			}
			// 現在見ている部分を枠で強調
			const auto& fr = callStack.back();
			cellRect(fr.p.x, fr.p.y).drawFrame(3, 0, ColorF(1.0, 0.2, 0.2, 0.9));
			// トップに印
			font(U"← top").draw(panelX + 160, panelY + (Min(total, maxStackLines) - 1) * 18, ColorF(0.25));
		}
		else {
			font(U"(空)").draw(panelX, panelY, ColorF(0.6));
		}


		// ==== 探索の進行 ====
		if (phase == Phase::Explore) {
			font(U"探索アニメ速度: {:.2f} s/step （Q/W で遅く/速く）"_fmt(stepInterval)).draw(gridOrigin.x, gridOrigin.y - 30, ColorF(0.1));
			if (stepSW.sF() >= stepInterval) {
				stepSW.restart();
				stepExplore();
			}
			// 速度変更
			if (KeyQ.down()) {
				stepInterval = Min(0.80, stepInterval + 0.05);
			}
			if (KeyW.down()) {
				stepInterval = Max(0.02, stepInterval - 0.02);
			}
		}

		// 完了フェーズ
		if (phase == Phase::Done) {
			fontTitle(U"探索完了！  [R] で準備フェーズへ戻る").draw(gridOrigin.x, gridOrigin.y - 30, ColorF(0.1));

			if (KeyR.down()) {
				// 訪問結果のみリセット
				phase = Phase::Prep;
				resetVisit();
			}
		}
	}
}
