# include <Siv3D.hpp>

// 盤面の大きさを変えたい場合はここを変更

#define CELLSIZE 88

struct Board {
	// ==== 盤面サイズ固定（6x6）====
	static constexpr int W = 6;
	static constexpr int H = 6;

	// ==== 盤面状態（ゲーム用）====
	Grid<bool> wall{ H, W, false };    // 壁なら true
	Grid<bool> visited{ H, W, false }; // 塗られているか
	Point start{ 0, 0 };
	Point player{ 0, 0 };
	int nonWallCount = 0;              // 壁でないマスの総数

	// ==== ユーティリティ ====
	static bool inBounds(int x, int y) { return (0 <= x && x < W && 0 <= y && y < H); }
	static bool inBounds(const Point& p) { return inBounds(p.x, p.y); }

	void setFromRows(const Array<String>& rows) {
		wall.fill(false);
		visited.fill(false);
		nonWallCount = 0;
		for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
			const char32 c = rows[y][x];
			if (c == U'#') wall[y][x] = true;
			else {
				++nonWallCount;
				if (c == U'S') start = { x, y };
			}
		}
		player = start;
		visited[start.y][start.x] = true;
	}

	void resetPaint() {
		visited.fill(false);
		visited[start.y][start.x] = true;
		player = start;
	}

	bool isObstacleGame(const Point& p) const {
		if (!inBounds(p)) return true;
		if (wall[p.y][p.x]) return true;
		if (visited[p.y][p.x]) return true;
		return false;
	}

	int paintedCount() const {
		int c = 0;
		for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
			if (!wall[y][x] && visited[y][x]) ++c;
		}
		return c;
	}
	bool isCleared() const { return paintedCount() == nonWallCount; }

	// ============================================================
	// ここから生成ロジック（Board クラス内）
	// ============================================================

	// 一意性を満たすまでリトライ
	// minCoverRatio: 壁でないマスの割合の下限
	bool generateRandom(double minCoverRatio = 0.9, int maxReseedTries = 8000) {
		Array<String> rows;
		for (int t = 0; t < maxReseedTries; ++t) {
			if (generateOnce(rows, minCoverRatio) && isUniqueSolution(rows)) {
				setFromRows(rows);
				return true;
			}
		}
		return false;
	}

private:
	// ランダム生成
	bool generateOnce(Array<String>& rows, double minCoverRatio) const {
		Array<String> g(H, String(W, U'#'));
		Array<Point> carved;

		// ゴールを決める
		Point goal{ Random(0, W - 1), Random(0, H - 1) };
		g[goal.y][goal.x] = U'.';
		carved << goal;

		// 方向ランダム、長さもランダム
		int stuck = 0;
		while (stuck < 12) {
			// 4方向ベクトル（右、下、左、上）
			Array<Point> Dirs = { Point{1,0}, Point{0,1}, Point{-1,0}, Point{0,-1} };

			Dirs.shuffle();
			bool moved = false;

			for (const auto& d : Dirs) {
				Point head = carved.back();
				// その方向に連続する壁数
				int maxLen = 0;
				Point cur = head + d;
				while (inBounds(cur) && g[cur.y][cur.x] == U'#') {
					++maxLen;
					cur += d;
				}
				if (maxLen == 0) continue;

				// [1..maxLen] からランダム
				const int L = Random(1, maxLen);
				Point p = head;
				for (int k = 0; k < L; ++k) {
					p += d;
					g[p.y][p.x] = U'.';
					carved << p;
				}
				moved = true;
				break;
			}
			if (!moved) ++stuck;
		}

		// 壁でない割合の計算
		const int freeCount = countNonWall(g);
		if (freeCount < static_cast<int>(minCoverRatio * (W * H))) {
			return false;
		}

		// スタートは最後の位置
		const Point start = carved.back();
		g[start.y][start.x] = U'S';

		rows = g;
		return true;
	}

	// 壁でない数
	static int countNonWall(const Array<String>& g) {
		int cnt = 0;
		for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
			if (g[y][x] != U'#') ++cnt;
		}
		return cnt;
	}

	// 一意性チェック（滑走ルール通りに全探索、解が2つ見つかったら一意ではない）
	static bool isUniqueSolution(const Array<String>& g) {
		Grid<bool> wall(H, W, false);
		Point start{ 0, 0 };
		int nonWall = 0;
		for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
			const char32 c = g[y][x];
			if (c == U'#') wall[y][x] = true;
			else {
				++nonWall;
				if (c == U'S') start = { x, y };
			}
		}

		Grid<bool> vis(H, W, false);
		vis[start.y][start.x] = true;

		auto isObs = [&](const Point& p) -> bool {
			if (!inBounds(p)) return true;
			if (wall[p.y][p.x]) return true;
			if (vis[p.y][p.x]) return true;
			return false;
		};

		// 4方向ベクトル（右、下、左、上）
		Array<Point> Dirs = { Point{1,0}, Point{0,1}, Point{-1,0}, Point{0,-1} };

		int solutions = 0;

		std::function<void(Point, int)> dfs = [&](Point head, int painted) {
			// 解が2つ見つかったら終了
			if (solutions >= 2) return;
			if (painted == nonWall) {
				++solutions;
				return;
			}
			// 可能手を列挙
			Array<Array<Point>> moves;
			for (auto d : Dirs) {
				Array<Point> path;
				Point cur = head + d;
				if (isObs(cur)) continue;
				while (!isObs(cur)) { path << cur; cur += d; }
				if (!path.isEmpty()) moves << path;
			}

			for (const auto& path : moves) {
				for (const auto& p : path) vis[p.y][p.x] = true;
				dfs(path.back(), painted + static_cast<int>(path.size()));
				for (const auto& p : path) vis[p.y][p.x] = false;
				if (solutions >= 2) return;
			}
		};

		dfs(start, 1);
		// 一意なら true, そうでなければ false (元々 1 通りは存在する)
		return (solutions == 1);
	}
};

// メイン関数（描画とプレイの事しか書いてないので見なくてOK）
void Main() {
	Window::Resize(860, 640);
	Scene::SetBackground(ColorF(0.97));
	Font font(18);
	Font fontTitle(28, Typeface::Bold);

	Board board;

	// 生成
	if (!board.generateRandom(0.72, 1000000)) {
		throw Error{ U"盤面の生成に失敗" };
	}

	const int cellSize = CELLSIZE;
	const Point gridSize{ Board::W * cellSize, Board::H * cellSize };
	const Point gridOrigin{ (Scene::Width() - gridSize.x) / 2, (Scene::Height() - gridSize.y) / 2 + 10 };

	auto cellRect = [&](int x, int y) {
		return Rect(gridOrigin.x + x * cellSize, gridOrigin.y + y * cellSize, cellSize);
	};

	auto drawBoard = [&] {
		for (int y = 0; y < Board::H; ++y) for (int x = 0; x < Board::W; ++x) {
			const Rect r = cellRect(x, y);
			if (board.wall[y][x]) r.draw(ColorF(0.24));
			else if (board.visited[y][x]) r.draw(ColorF(0.98, 0.69, 0.25));
			else r.draw(ColorF(0.90));
			r.drawFrame(2, 0, ColorF(0.2, 0.2, 0.25));
		}
		// スタートマス
		const Rect rs = cellRect(board.start.x, board.start.y);
		rs.drawFrame(4, 0, ColorF(0.1, 0.6, 0.9));
		fontTitle(U"S").drawAt(rs.center(), ColorF(0.1, 0.5, 0.9));

		// プレイヤーマス
		const Rect rp = cellRect(board.player.x, board.player.y);
		const Circle c{ rp.center(), cellSize * 0.32 };
		c.draw(ColorF(0.2, 0.2, 0.3));
		c.drawFrame(2, ColorF(0.05, 0.05, 0.1));
	};

	while (System::Update()) {
		const bool cleared = board.isCleared();

		// 入力
		if (KeySpace.down()) {
			board.resetPaint(); // 戻る
		}
		if (KeyG.down()) {
			// 生成
			if (!board.generateRandom(0.72, 10000000)) {
				throw Error{ U"盤面の生成に失敗" };
			}
		}

		if (!cleared) {
			Optional<Point> dir;
			if (KeyUp.down()) dir = Point{ 0, -1 };
			else if (KeyDown.down()) dir = Point{ 0, 1 };
			else if (KeyLeft.down()) dir = Point{ -1, 0 };
			else if (KeyRight.down()) dir = Point{ 1, 0 };

			if (dir) {
				Point next = board.player + *dir;
				if (!board.isObstacleGame(next)) {
					while (!board.isObstacleGame(next)) {
						board.player = next;
						board.visited[board.player.y][board.player.x] = true;
						next += *dir;
					}
				}
			}
		}

		// 描画
		drawBoard();

		// UI
		Rect{ 0, 0, Scene::Width(), 64 }.draw(ColorF(1.0, 0.97));
		if (cleared) {
			fontTitle(U"✔ クリア！  [Space] リセット   [G] 盤面生成").draw(20, 16, Palette::Darkgreen);
		}
		else {
			fontTitle(U"塗り: {}/{}   [↑↓←→] 移動 / [Space] 戻る / [G] 生成"_fmt(board.paintedCount(), board.nonWallCount)).draw(20, 16, Palette::Black);
		}
	}
}
