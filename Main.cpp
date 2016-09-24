
# include <Siv3D.hpp>
# include <HamFramework.hpp>

struct Commondata
{
	const Texture texture_earth{ L"Data/Earth.png" };

	const Sound start{ L"Data/Start.wav" };

	const Font font{ 20, Typeface::Bold };

	const Font dialoguefont{ 10 };

	int32 totalscore, hiscore_s = 0, hiscore_e = 0;

	String gamemode;

	Array<Circle> stars;
};

using Myapp = SceneManager<String, Commondata>;

namespace ptt {			//念のためのnamespace
	enum class Mode
	{
		Ready = 0,
		Poke,
		Score
	};

	struct HitEffect :IEffect		//棒で月を突いたときのエフェクト
	{
		struct Particle
		{
			Particle() = default;
			Particle(double _x, double _y, Vec2 _move)
				:base(_x, _y, 3), move(_move) {}
			Circle base;
			Vec2 move;
		};

		Array<ptt::HitEffect::Particle> effects;

		HitEffect(int32 _angle)
		{
			const auto x = Cos(Radians(_angle - 90)) * 200 + Window::Center().x;	//-90と200は月の初期位置からなる定数。
			const auto y = Sin(Radians(_angle - 90)) * 200 + Window::Center().y;	//本来なら計算で求めるべきところだけど手抜き。
			for (int i = 0;i < 10;i++)
			{
				const auto vec = Vec2(Cos(Radians(Random(90 + _angle, 270 + _angle))), Sin(Radians(Random(90 + _angle, 270 + _angle)))).setLength(Random(1.0, 3.0));
				effects.emplace_back(x, y, vec);
			}
		}

		bool update(double t) override
		{
			for (auto& effect : effects)
			{
				effect.base.moveBy(effect.move);
				effect.base.draw(ColorF(1, 1, 0, 1.0 - t));
			}
			return t < 1.0;
		}
	};
}

/*ふきだしの構造体*/
struct CharDialogue
{
	CharDialogue() = default;
	CharDialogue(double _x, double _y, double _w, String _t)
		:baloon(_x, _y, _w, 30, 4)
		, text(_t) {}

	RoundRect baloon;
	String text;
};

/*タイトル画面*/
struct Title :Myapp::Scene
{
	const Circle circle{ Window::Center().movedBy(0,240),600 };

	const Rect scoreattack{ Window::Center().movedBy(-120,-20),240,40 },
		endless{ Window::Center().movedBy(-120,40),240,40 };

	double angle = 0;

	void init() override
	{
		if (m_data->stars.empty())
		{
			for (auto i : step(200)) 
			{
				m_data->stars.emplace_back(RandomVec2(circle), 2);
			}
		}
	}

	void update() override
	{
		angle = angle < -TwoPi ? 0 : angle -= 0.001;
		if (scoreattack.leftClicked)
		{
			m_data->gamemode = L"スコアアタック";
			m_data->start.play();
			changeScene(L"Game");
		}
		if (endless.leftClicked)
		{
			m_data->gamemode = L"エンドレス";
			m_data->start.play();
			changeScene(L"Game");
		}
	}

	void draw() const override
	{
		Rect(Window::ClientRect()).draw({ Color(0),Color(0),Palette::Steelblue,Palette::Steelblue });
		for (const auto star : m_data->stars)
		{
			const Transformer2D transformer(Mat3x2::Rotate(angle, Window::Center().movedBy(0, 240)));
			star.draw();
		}
		Circle(Window::Center().movedBy(0, 480), 360).draw(Color(0));

		scoreattack.draw(scoreattack.mouseOver ? AlphaF(0.2) : AlphaF(0));
		endless.draw(endless.mouseOver ? AlphaF(0.2) : AlphaF(0));

		m_data->font(L"Tsuki x Tsuki").drawAt(Window::Center().movedBy(0, -100));
		m_data->font(L"スコアアタック").drawAt(scoreattack.center);
		m_data->font(L"エンドレス").drawAt(endless.center);
	}
};

/*メインのゲーム本体*/
struct Game :Myapp::Scene
{
	const Rect earth{ Window::Center().movedBy(-100,-100),200 };
	const Circle moon{ Window::Center().movedBy(0,-200),25 };
	const Rect stick{ moon.center.movedBy(-100, -10).asPoint(), 60, 20 };

	Array<String> dialogueues
	{
		L"お月見したいなあ",L"月が見たいのう",L"お月さま見えないね",L"まだ月が出てないな",
		L"パパお月さまとって",L"今日は新月なの？",L"月見が日課なんだ",L"あれ？お月さまは？"
	};

	Array<CharDialogue> characters;

	const Polygon hexagon
	{
		{ 120,160 },{ 320,120 },{ 520,160 }
		,{ 520,320 },{ 320,360 },{ 120,320 }
	};

	Rect bar{ Window::Center().movedBy(-80, -23), 10, 46 };

	Stopwatch stopwatch_bar, stopwatch_poke, stopwatch_next, stopwatch_end;

	EasingController<double> easy{ 0.0, 1.0, Easing::Sine, 300.0 };
	EasingController<double> bareasy{ 0.0, 150.0, Easing::Sine, 400.0 };
	EasingController<double> stickeasy{ 0.0, 6.0, Easing::Quart, 400.0 };
	EasingController<double> mooneasy{ memory,angle,Easing::Cubic,500.0 };

	int32 theta, angle, memory, answer, score, total, remainingtime;
	bool flag_stick, flag_poke, flag_scene;

	ptt::Mode mode;

	Effect effect;

	void init() override
	{
		theta = angle = memory = score = total = 0;
		remainingtime = 30;
		flag_stick = flag_scene = false;
		flag_poke = true;
		mode = ptt::Mode::Ready;
		stopwatch_end.start();
	}

	void update() override
	{
		switch (mode)
		{
		//ここからふきだし
		case ptt::Mode::Ready:
		{
			if (!characters.empty()) characters.clear(); //削除

			answer = Random(0, 359);
			const auto _pos = Window::Center().movedBy(Vec2(Cos(Radians(answer)), Sin(Radians(answer))).setLength(150).asPoint());
			const auto _text = dialogueues[Random(dialogueues.size() - 1)];
			const auto _len = _text.length * 15 + 10;
			
			if (_pos.x < Window::Center().x && _pos.y < Window::Center().y)
				characters.emplace_back(_pos.x - _len, _pos.y - 30, _len, _text);	//左上

			if (_pos.x < Window::Center().x && _pos.y >= Window::Center().y)
				characters.emplace_back(_pos.x - _len, _pos.y, _len, _text);		//左下
			
			if (_pos.x >= Window::Center().x && _pos.y < Window::Center().y)
				characters.emplace_back(_pos.x, _pos.y - 30, _len, _text);			//右上
			
			if (_pos.x >= Window::Center().x && _pos.y >= Window::Center().y)
				characters.emplace_back(_pos.x, _pos.y, _len, _text);				//右下
			
			mode = ptt::Mode::Poke;
		}
			break;
			
		//ここから突き
		case ptt::Mode::Poke:
			if (theta != 0 && easy.easeInOut() == 0)
			{
				memory = angle = angle % 360;
				angle += theta;
				theta = 0;
				mooneasy = EasingController<double>(memory, angle, Easing::Cubic, 500.0);
				flag_stick = true;
				stickeasy.start();
			}

			if (flag_stick)
			{
				stickeasy.start();
			}
			if (stickeasy.easeInOut() == 6.0)
			{
				flag_stick = false;
				flag_scene = true;
				effect.add<ptt::HitEffect>(memory);
				mooneasy.start();
			}

			if (flag_poke && Input::MouseL.clicked)
			{
				if (easy.easeInOut() == 0)
				{
					easy.start();
					stopwatch_bar.start();
				}

				if (easy.easeInOut() == 1.0)
				{
					flag_poke = false;
					bareasy.pause();
					stopwatch_bar.reset();
					theta = Max(static_cast<int32>(bareasy.easeInOut() / 150.0 * 360.0), 1);
					stopwatch_poke.start();
				}
			}

			if (stopwatch_bar.ms() >= 300.0)
			{
				bareasy.start();
			}
			if (stopwatch_poke.ms() >= 1000)
			{
				stopwatch_poke.reset();
				bareasy = EasingController<double>(0.0, 150.0, Easing::Sine, 400.0);
				easy.start();
			}

			if (flag_scene ? mooneasy.easeInOut() == mooneasy.getEnd() : false)  mode = ptt::Mode::Score;	//case遷移

			break;

		//ここからスコア
		case ptt::Mode::Score:
		{
			if ((angle - 90) % 360 >= answer)
				score = ((180 - Min(Abs(((angle - 90) % 360) - answer), Abs((answer + 360) - ((angle - 90) % 360)))) * 100) / 180;
			else
				score = ((180 - Min(Abs(answer - ((angle - 90) % 360)), Abs((((angle - 90) % 360) + 360) - answer))) * 100) / 180;

			if (m_data->gamemode == L"エンドレス") remainingtime += score / 20;

			total += score;

			stopwatch_next.start();

			flag_scene = false;
			flag_poke = true;
			mode = ptt::Mode::Ready;
		}
			break;
		default:
			break;
		}
		if (stopwatch_next.ms() > 1000)stopwatch_next.reset();
		
		if (m_data->gamemode == L"スコアアタック" && stopwatch_end.s() >= 60)
		{
			m_data->totalscore = total;
			m_data->hiscore_s = (total > m_data->hiscore_s ? total : m_data->hiscore_s);
			changeScene(L"Result");
		}
		if (m_data->gamemode == L"エンドレス" && stopwatch_end.s() >= remainingtime)
		{
			m_data->totalscore = total;
			m_data->hiscore_e = (total > m_data->hiscore_e ? total : m_data->hiscore_e);
			changeScene(L"Result");
		}
	}

	void draw() const override
	{
		for (const auto star : m_data->stars)star.draw();
		earth(m_data->texture_earth).draw();

		for (const auto character : characters)
		{
			character.baloon.draw();
			m_data->dialoguefont(character.text).drawAt(character.baloon.center, Color(0));
		}

		{
			const Transformer2D transformer(Mat3x2::Rotate(Radians(memory + stickeasy.easeIn()), Window::Center()));
			stick.draw(Palette::Brown);
		}

		{
			const Transformer2D transformer(Mat3x2::Rotate(Radians(mooneasy.easeInOut()), Window::Center()));
			moon.draw(Palette::Yellow);
		}

		m_data->dialoguefont(L"残り", m_data->gamemode == L"スコアアタック" ? (60 - stopwatch_end.s()) : (remainingtime - stopwatch_end.s()), L"秒").draw(0, 0);
		m_data->dialoguefont(L"TOTAL:", total, L"点").draw(500, 0);
		if (stopwatch_next.isActive())
		{
			m_data->dialoguefont(L"+", score, L"点").draw({ 550, 20 }, AlphaF(1 - static_cast<double>(stopwatch_next.ms()) / 1000));
			if (m_data->gamemode == L"エンドレス")m_data->dialoguefont(L"+", score / 20, L"秒").draw({ 20, 20 }, AlphaF(1 - static_cast<double>(stopwatch_next.ms()) / 1000));
		}

		effect.update();

		{
			const Transformer2D transformer(Mat3x2::Translate(-Window::Center()).scale(easy.easeInOut())
				.translate(Window::Center()).rotate(easy.easeInOut()*TwoPi, Window::Center()));

			hexagon.draw(ColorF(0.4, 0.6, 0.8, 0.3));

			m_data->font(L"よく狙ってクリック！！").drawAt(Window::Center().movedBy(0, -60), ColorF(1.0, 0.4, 0));

			if (stopwatch_poke.isActive())
			{
				m_data->font(L"Poke the Moon!!").drawAt(Window::Center().movedBy(0, 60), ColorF(1.0, 0.4, 0));
			}

			Rect(150, 40).setCenter(Window::Center()).draw({ HSV(0,0,1),HSV(0), HSV(0), HSV(0,0,1) }).drawFrame(1.0, 0.0, Color(0));

			bar.movedBy(static_cast<int>(bareasy.easeInOut()), 0).draw(Palette::Yellow).drawFrame(1, 0, ColorF(0.3));
		}
	}
};

/*リザルト*/
struct Result :Myapp::Scene
{
	double angle = 0;

	void update() override
	{
		angle = angle < -TwoPi ? 0 : angle -= 0.001;
		if (Input::MouseL.clicked)changeScene(L"Title");
	}

	void draw() const override
	{
		Rect(Window::ClientRect()).draw({ Color(0),Color(0),Palette::Steelblue,Palette::Steelblue });
		for (const auto star : m_data->stars)
		{
			const Transformer2D transformer(Mat3x2::Rotate(angle, Window::Center().movedBy(0, 240)));
			star.draw();
		}
		Circle(Window::Center().movedBy(0, 480), 360).draw(Color(0));

		m_data->font(L"あなたの得点は\n", m_data->totalscore, L"点でした！").drawAt(Window::Center().movedBy(0, -40));
		m_data->font(m_data->gamemode, L"のハイスコア:", m_data->gamemode == L"スコアアタック" ? m_data->hiscore_s : m_data->hiscore_e, L"点").drawAt(Window::Center().movedBy(0, 40));
		m_data->dialoguefont(L"クリックでタイトルに戻ります").drawAt(Window::Center().movedBy(0, 80));
	}
};

void Main()
{
	Window::Resize(640, 480);
	Window::SetTitle(L"Tsuki x Tsuki ～突きで月を見せよう～");
	Graphics::SetBackground(Color(0));
	
	Myapp manager;
	manager.add<Title>(L"Title");
	manager.add<Game>(L"Game");
	manager.add<Result>(L"Result");

	while (System::Update())
	{
		if (!manager.updateAndDraw())
			break;
	}
}
