#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <time.h>
#include <random>
#include <chrono>
#include <format>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/System/Clock.hpp>

using namespace std;

mt19937 mrand(time(nullptr));
sf::Font font;


struct quad {
	uint8_t r, g, b, a;
	quad(float r, float g, float b) : r(int(r)), g(int(g)), b(int(b)), a(255) {}
	void set(sf::Uint8* colors, int i) { // ставит весь пиксель на нужное место
		colors[i] = r;
		colors[i+1] = g;
		colors[i+2] = b;
		colors[i+3] = a;
	}
};
// hue: 0-360°; sat: 0.f-1.f; val: 0.f-1.f
quad hsv(int hue, float sat, float val) {

	hue %= 360;
	while (hue < 0) hue += 360;

	if (sat < 0.f) sat = 0.f;
	if (sat > 1.f) sat = 1.f;

	if (val < 0.f) val = 0.f;
	if (val > 1.f) val = 1.f;

	int h = hue / 60;
	float f = float(hue) / 60 - h;
	float p = val * (1.f - sat);
	float q = val * (1.f - sat * f);
	float t = val * (1.f - sat * (1 - f));

	switch (h) {
	default:
	case 0:
	case 6: return { val * 255, t * 255, p * 255 };
	case 1: return { q * 255, val * 255, p * 255 };
	case 2: return { p * 255, val * 255, t * 255 };
	case 3: return { p * 255, q * 255, val * 255 };
	case 4: return { t * 255, p * 255, val * 255 };
	case 5: return { val * 255, p * 255, q * 255 };
	}
}

int textFormat(sf::Text& t, int w) { // подгоняет размер шрифта под нужную ширину свободного места
	int csize = 10;
	t.setCharacterSize(10);
	while (t.getGlobalBounds().width + 15 < w) {
		csize++;
		t.setCharacterSize(csize);
	}
	csize--;
	t.setCharacterSize(csize);
	return csize;
}

class Shelling
{
private:
	unsigned n;
	unsigned m;
	vector<vector<int>> field;
	vector<pair<int, int>> updateOrder;
	float p0 = 0.2; //вероятность пустых клеток
	int colours = 0; // общее количество цветов (не считая пустого)
	bool uniformProbs = true; // равномерное распределение цветов
	bool uniformTolerance = true;
	vector<float> probs; // если неравномерное
	vector<int> tolerance = { 7 }; //терпимость, макс. кол-во чужих вокруг для каждой группы
	const float maxr = float(2 << 16);
	int r = 2; // радиус квадрата проверки чужих

	sf::Uint8* pixels;
	sf::Texture texture;

	vector<int> free; // массив пустых клеток

	//меняем блоки из 4 Uint8 в pixels
	void swap4(int i, int j) {
		swap(pixels[4 * i], pixels[4 * j]);
		swap(pixels[4 * i + 1], pixels[4 * j + 1]);
		swap(pixels[4 * i + 2], pixels[4 * j + 2]);
		swap(pixels[4 * i + 3], pixels[4 * j + 3]);
	};

public:
	string getInfo() {
		return "Параметры:\n p0 = " + format("{:.2f}", p0) + "\nКол-во цветов = " + to_string(colours) + "\nТерпимость = " + to_string(tolerance[0]) + "\nРадиус = " + to_string(r);
	}
	// float p0, int colours, int t, int r
	void setParam(int paramId, bool inc, int i = 0) {
		switch (paramId) {
		case 0:
			if (inc)
				p0 = min(0.95, p0 + 0.05);
			else
				p0 = max(0.03, p0 - 0.05);
			setup();
			break;
		case 1:
			if (inc) {
				colours++;
				tolerance.push_back(tolerance[0]);
			}
			else {
				colours = max(2, colours - 1);
				if (colours > 2)tolerance.pop_back();
			}
			setup();

			break;
		case 2:
			if (inc)
				tolerance[i]++;
			else
				tolerance[i] = max(0, tolerance[i] - 1);
			break;
		case 3:
			if (inc)
				r++;
			else
				r = max(1, r - 1);
			break;
		}

	}

	int getColours() { return colours; }
	vector<int>& getTolerance() {
		return tolerance;
	}
	void setUniformTolerance(bool mode) {
		uniformTolerance = mode;
	}
	void setup() {

		free.clear();

		for (int i = 0; i < n; ++i) {
			for (int j = 0; j < m; ++j) {

				float rt = float(mrand() % int(maxr)) / maxr;

				if (rt < p0) {
					field[i][j] = 0;
					free.push_back(i * m + j);
				}
				else if (uniformProbs) {
					field[i][j] = abs(int((rt - p0) / (1 - p0) * colours)) + 1;
				}
				else {
					int k = 0;
					float t = p0;
					while (t < rt) {
						t += probs[k];
						k++;
					}
					field[i][j] = k;
				}

				quad col = hsv(field[i][j] * 360 / colours, 0.95 * (field[i][j] != 0), 0.95 * (field[i][j] != 0));
				col.set(pixels, 4 * (i * m + j));
			}
		}

	}

	void draw(sf::RenderWindow& window)
	{
		static sf::Sprite sprite(texture);
		texture.update(pixels);
		sprite.setPosition(window.getSize().x / 4, 0);
		sprite.setScale(sf::Vector2f(2, 2));

		window.draw(sprite);
	}

	pair<int, int> countNear(int x, int y)
	{
		//int w = m, h = n;
		if (field[x][y] == 0) return { 0, 0 }; // для пустых клеток нет врагов
		int total = 0, target = 0, targetColor = field[x][y];
		for (int i = x - r; i <= x + r; i++)
		{
			for (int j = y - r; j <= y + r; j++)
			{
				int c = field[(i + n) % n][(j + m) % m]; // цвет в клетке с цикличными координатами
				if (c > 0) total++;
				if (c == targetColor) target++;
			}
		}
		return { total - target, targetColor };
	}

	void partUpdate(int starti, int startj, int height, int width) {
		for (int i = starti; i < starti + height; ++i)
		{
			for (int j = startj; j < startj + width; ++j)
			{
				auto [nearby, target] = countNear(i, j);
				int currentTolerance = uniformTolerance ? tolerance[0] : tolerance[target];
				if (nearby >= currentTolerance)
				{
					int rndk = mrand() % free.size();
					int k = free[rndk];
					swap(field[i][j], field[k / m][k % m]);
					swap4(i * m + j, k);
					free[rndk] = i * m + j;
				}
			}
		}
	}
	void randomUpdate() {
		for (auto [i, j] : updateOrder) {
			auto [nearby, target] = countNear(i, j);
			int currentTolerance = uniformTolerance ? tolerance[0] : tolerance[target];
			if (nearby >= currentTolerance)
			{
				int rndk = mrand() % free.size();
				int k = free[rndk];
				swap(field[i][j], field[k / m][k % m]);
				swap4(i * m + j, k);
				free[rndk] = i * m + j;
			}
		}
	}
	void update()
	{
		//partUpdate(0, 0, n, m);
		//shuffle(updateOrder.begin(), updateOrder.end(), mrand);
		randomUpdate();
	}

	Shelling(unsigned n, unsigned m, float p0, int colours, int tl) : n(n), m(m), p0(p0), colours(colours)
	{
		field.resize(n, vector<int>(m));
		tolerance.resize(colours + 1, tl);
		texture.create(m, n);
		pixels = new sf::Uint8[n * m * 4];

		for (int i = 0; i < n; i++) {
			for (int j = 0; j < m; ++j) {
				updateOrder.push_back({ i, j });
			}
		}
		shuffle(updateOrder.begin(), updateOrder.end(), mrand);

		setup();
	}
	Shelling(unsigned n, unsigned m, float p0, vector<float> probs, int tl) : n(n), m(m), p0(p0), colours(probs.size()), probs(probs)
	{
		uniformProbs = false;
		tolerance.resize(colours + 1, tl);
		field.resize(n, vector<int>(m));
		texture.create(m, n);
		pixels = new sf::Uint8[n * m * 4];
		setup();
	}
	~Shelling() {
		delete[] pixels;
	}

};

class Button{
private:
	int left, top, width, height;
	sf::RectangleShape rect;
	sf::IntRect hitbox;
	string text;
	sf::Text title;
	bool state = 0, click = 0;
	bool locked = false;
	sf::RenderWindow& window;
public:
	int id = -1;
	
	Button(int left, int top, int width, int height, string text, sf::RenderWindow& w) : 
		left(left), top(top), width(width), height(height), text(text), window(w) {


		rect.setSize(sf::Vector2f(width, height));
		rect.setPosition(sf::Vector2f(left, top));

		title.setFont(font);
		title.setString(sf::String::fromUtf8(text.begin(), text.end()));
		textFormat(title, width);
		int xoffset = (width - title.getGlobalBounds().width) / 2, yoffset = (height - title.getGlobalBounds().height) / 2;
		title.setPosition(left + xoffset, top + yoffset);
		title.setFillColor(sf::Color::Black);

		hitbox = sf::IntRect(left, top, width, height);
	}
	bool handle() {
		click = 0;
		if (hitbox.contains(sf::Mouse::getPosition(window))) {
			if (!locked)
				rect.setFillColor(sf::Color(227, 212, 188));
			if (state && !sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
				state = 0;
				click = 1;
			} 
			if (!state && sf::Mouse::isButtonPressed(sf::Mouse::Left)) state = 1;
		}
		else {
			if (!locked)
				rect.setFillColor(sf::Color(245, 237, 225));
			state = 0;
			
		}
		
		window.draw(rect);
		window.draw(title);
		return click;
	}
	bool isLocked() { return locked; }
	void lock() {
		locked = 1;
		rect.setFillColor(sf::Color(153, 145, 133));
		window.draw(rect);
		window.draw(title);
	}
	void unlock() {
		locked = 0;
		rect.setFillColor(sf::Color(245, 237, 225));
		window.draw(rect);
		window.draw(title);
	}


};

class Selector {
private:
	int cnt;
	int selected = 0;
	int size;
	int Hpos, Wpos;
	vector<sf::RectangleShape> options;
	vector<sf::Text> descriptions;
	sf::RectangleShape SelectSquare;
	Shelling& model;
public:
	Selector(int number, int s, int w, int h, Shelling& model) : cnt(number + 1), size(s), Wpos(w), Hpos(h), model(model) {
		options.resize(cnt, sf::RectangleShape());
		descriptions.resize(cnt, sf::Text());
		for (int i = 0; i < options.size(); ++i) {
			options[i].setSize(sf::Vector2f(size, size));
			options[i].setPosition(Wpos + 2 * size * i, Hpos);
			auto [r, g, b, a] = hsv(i * 360 / (cnt - 1), 0.95 * (i != 0), 0.95 * (i != 0));
			options[i].setFillColor(sf::Color(r, g, b, a));

			descriptions[i].setFont(font);
			descriptions[i].setCharacterSize(size);
			descriptions[i].setFillColor(sf::Color::Black);
			descriptions[i].setPosition(Wpos + 2 * size * i, Hpos + 2 * size);
			descriptions[i].setString(to_string(model.getTolerance()[i]));
		}

		SelectSquare.setSize(sf::Vector2f(1.4 * size, 1.4 * size));
		SelectSquare.setPosition(sf::Vector2f(Wpos - 0.2 * size, Hpos - 0.2 * size));
		SelectSquare.setFillColor(sf::Color(0, 0, 0, 50));
		
	}
	int getSelected() { return selected; }
	void draw(sf::RenderWindow& window) {
		for (int i = 0; i < options.size(); ++i) {
			window.draw(options[i]);
			descriptions[i].setString(to_string(model.getTolerance()[i]));
			window.draw(descriptions[i]);
		}
		
		window.draw(SelectSquare);
	}
	void MoveLeft() {
		selected = (selected - 1 + cnt) % cnt;
		SelectSquare.setPosition(sf::Vector2f(Wpos - 0.2 * size + 2 * size * selected, Hpos - 0.2 * size));
	}
	void MoveRight() {
		selected = (selected + 1) % cnt;
		SelectSquare.setPosition(sf::Vector2f(Wpos - 0.2 * size + 2 * size * selected, Hpos - 0.2 * size));
	}
	void push() {
		
		options.push_back(sf::RectangleShape());
		options[cnt].setSize(sf::Vector2f(size, size));
		options[cnt].setPosition(Wpos + 2 * size * cnt, Hpos);
		descriptions.push_back(sf::Text());
		descriptions[cnt].setFont(font);
		descriptions[cnt].setCharacterSize(size);
		descriptions[cnt].setFillColor(sf::Color::Black);
		descriptions[cnt].setPosition(Wpos + 2 * size * cnt, Hpos + 2 * size);
		descriptions[cnt].setString(to_string(model.getTolerance()[cnt]));
		cnt++;
		for (int i = 0; i < options.size(); ++i) {
			auto [r, g, b, a] = hsv(i * 360 / (cnt - 1), 0.95 * (i != 0), 0.95 * (i != 0));
			options[i].setFillColor(sf::Color(r, g, b, a));
		}

		selected %= cnt;
		SelectSquare.setPosition(sf::Vector2f(Wpos - 0.2 * size + 2 * size * selected, Hpos - 0.2 * size));
	}
	void pop() {
		if (cnt > 3) { options.pop_back(); descriptions.pop_back(); }
		cnt = max(3, cnt - 1);
		for (int i = 0; i < options.size(); ++i) {
			auto [r, g, b, a] = hsv(i * 360 / (cnt - 1), 0.95 * (i != 0), 0.95 * (i != 0));
			options[i].setFillColor(sf::Color(r, g, b, a));
		}
		selected = min(cnt-1, selected);
		SelectSquare.setPosition(sf::Vector2f(Wpos - 0.2 * size + 2 * size * selected, Hpos - 0.2 * size));
	}

};



class Menu {
private:
	sf::RenderWindow& window;
	Shelling& model;
	Button* Reset, * Exit, * Colour, * Radius, * Tolerance, * Prob0,
		*Increase, *Decrease, *Left, *Right;
	vector<Button*> SelectableButtons; //набор указателей на кпоки-переключатели
	Button* LastLocked;
	Selector* ColorSelector;
	sf::Text ModelInfo, ToleranceInfo;
	int H, W, margin, blockOffset;
	string Info;
public:
	Menu(sf::RenderWindow& w, Shelling& model, int width, int height) : window(w), model(model), W(width), H(height) {
		margin = H / 80, blockOffset = H / 30;
		Reset = new Button(margin, margin, W / 10, H / 8, "Заново", window);
		Exit = new 	Button(margin, H - margin - H / 11, W / 16, H / 11, "Выход", window);

		Colour = new Button(margin, margin + H / 8 + 2 * blockOffset, W / 10, H / 10, "Кол-во цветов", window);
		Radius = new Button(margin, margin + H / 8 + H / 10 + 3 * blockOffset, W / 10, H / 10, "Радиус", window);
		Tolerance = new Button(margin, margin + H / 8 + H / 5 + 4 * blockOffset, W / 10, H / 10, "Терпимость", window);
		Prob0 = new Button(margin, margin + H / 8 + 3 * H / 10 + 5 * blockOffset, W / 10, H / 10, "Вероятность 0", window);
		//     0            1       2      3
		// float p0, int colours, int t, int r
		Prob0->id = 0;  Colour->id = 1; Tolerance->id = 2; Radius->id = 3;
		SelectableButtons = { Colour, Radius, Tolerance, Prob0 };
		LastLocked = Colour;

		Increase = new Button(margin + W / 10 + blockOffset, margin + H / 8 + 2 * blockOffset, W / 10, H / 7, "Увеличить", window);
		Decrease = new Button(margin + W / 10 + blockOffset, margin + H / 8 + H / 7 + 3 * blockOffset, W / 10, H / 7, "Уменьшить", window);

		Left = new Button(margin + W / 10 + blockOffset, margin + H / 8 + 2 * H / 7 + 4 * blockOffset, W / 21, H / 7, "Влево", window);
		Right = new Button(margin + W / 10 + W / 30 + 2 * blockOffset, margin + H / 8 + 2 * H / 7 + 4 * blockOffset, W / 21, H / 7, "Вправо", window);

		ColorSelector = new Selector(model.getColours(), W / 80, margin, margin + H / 8 + 3 * H / 7 + 5 * blockOffset, model);


		ModelInfo.setFont(font);
		Info = model.getInfo();
		ModelInfo.setString(sf::String::fromUtf8(Info.begin(), Info.end()));
		ModelInfo.setFillColor(sf::Color::Black);
		ModelInfo.setPosition(margin + W / 10 + blockOffset, margin);
		textFormat(ModelInfo, W / 9);

		/*ToleranceInfo.setFont(font);
		ToleranceInfo.setFillColor(sf::Color::Black);
		ToleranceInfo.setPosition(margin, margin + H / 8 + 3 * H / 7 + 5 * blockOffset + W / 40);
		ToleranceInfo.setString(model.getTolerance());
		ToleranceInfo.setCharacterSize(W / 60);*/
	}
	void draw() {
		if (Reset->handle()) model.setup();
		if (Exit->handle()) window.close();

		for (Button* bt : SelectableButtons) { // проходимся по кнопкам, которые могут быть выбраны
			if (bt->handle()) { //если нажали, то меняем её состояние и отжимаем предыдущую
				if (bt->isLocked())
					bt->unlock();
				else {
					LastLocked->unlock();
					bt->lock();
					LastLocked = bt;
				}

			}
		}

		if (Increase->handle()) {
			if (LastLocked->isLocked()) {
				model.setParam(LastLocked->id, 1, ColorSelector->getSelected());
				if (LastLocked->id == 1) ColorSelector->push();
			}
		}
		if (Decrease->handle()) {
			if (LastLocked->isLocked()) {
				model.setParam(LastLocked->id, 0, ColorSelector->getSelected());
				if (LastLocked->id == 1) ColorSelector->pop();
			}
		}

		if (Left->handle()) {
			ColorSelector->MoveLeft();
			model.setUniformTolerance(!ColorSelector->getSelected());
		}
		if (Right->handle()) {
			ColorSelector->MoveRight();
			model.setUniformTolerance(!ColorSelector->getSelected());
		}

		ColorSelector->draw(window);

		Info = model.getInfo();
		ModelInfo.setString(sf::String::fromUtf8(Info.begin(), Info.end()));
		//ToleranceInfo.setString(model.getTolerance());
		window.draw(ModelInfo);
		window.draw(ToleranceInfo);
	}
};

class FPSmeter {
public:
	sf::Text FPS;
	sf::Clock clock;
	float lastTime = clock.getElapsedTime().asSeconds(), currentTime;
	FPSmeter(float x, float y, int c) {
		FPS.setFont(font);
		FPS.setCharacterSize(c);
		FPS.setPosition(x, y);
		FPS.setFillColor(sf::Color::Black);
	}
	int draw(sf::RenderWindow& window) {
		currentTime = clock.getElapsedTime().asSeconds();
		int fps = 1.f / (currentTime - lastTime);
		lastTime = currentTime;
		FPS.setString(to_string(fps));
		window.draw(FPS);
		return fps;
	}
};

int main() {

	if (!font.loadFromFile("res/Mulish.ttf"))
	{
		cout << "Font wasn't loaded\n";
		return 1;
	}

	unsigned int W = sf::VideoMode::getDesktopMode().width;
	unsigned int H = sf::VideoMode::getDesktopMode().height;

	Shelling model(H / 2, W * 3/8, 0.02, 2, 7); // изображение занимает весь экран по высоте и 3/4 по ширине, но рендерим в 4 раза меньше для производительности

	sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "Schelling segregation", sf::Style::Fullscreen);

	Menu menu(window, model, W, H);

	FPSmeter FPS(H/80, H/80 + H / 8 + H / 60, W / 60);

	while (window.isOpen())
	{
		
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}
		
		window.clear(sf::Color::White);

		model.update();
		model.draw(window);

		menu.draw();

		FPS.draw(window);

		window.display();
	}

	return 0;
}