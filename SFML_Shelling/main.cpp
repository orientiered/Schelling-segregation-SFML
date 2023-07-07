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

class Shelling
{
private:
	unsigned n;
	unsigned m;
	vector<vector<int>> field;
	float p0 = 0.2; //вероятность пустых клеток
	int colours = 0; // общее количество цветов (не считая пустого)
	bool uniform = true; // равномерное распределение цветов
	vector<float> probs; // если неравномерное
	const float maxr = float(2 << 16);
	int t = 3; // терпимость, макс. количество чужих вокруг
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
		return "Параметры:\n p0 = " + format("{:.2f}", p0) + "\nКол-во цветов = " + to_string(colours) + "\nТерпимость = " + to_string(t) + "\nРадиус = " + to_string(r);
	}
	// float p0, int colours, int t, int r
	void setParam(int paramId, bool inc) {
		switch (paramId) {
		case 0:
			if (inc)
				p0 = min(0.95, p0 + 0.05);
			else
				p0 = max(0.03, p0 - 0.05);
			break;
		case 1:
			if (inc)
				colours++;
			else
				colours = max(2, colours - 1);
			break;
		case 2:
			if (inc)
				t++;
			else
				t = max(0, t - 1);
			break;
		case 3:
			if (inc)
				r++;
			else
				r = max(1, r - 1);
			break;
		}
		setup();
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
				else if (uniform) {
					field[i][j] = abs(int((rt-p0)/(1-p0) * colours)) + 1;
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
		sprite.setPosition(window.getSize().x/4, 0);
		sprite.setScale(sf::Vector2f(2,2));
		
		window.draw(sprite);
	}

	int countNear(int x, int y)
	{
		//int w = m, h = n;
		if (field[x][y] == 0) return 0; // для пустых клеток нет врагов
		int total = 0, target = 0, targetColor = field[x][y];
		for (int i = x - r; i < x + r; i++)
		{
			for (int j = y - r; j < y + r; j++)
			{
				int c = field[(i + n) % n][(j + m) % m]; // цвет в клетке с цикличными координатами
				if (c > 0) total++;
				if (c == targetColor) target++;
			}
		}
		return total - target;
	}

	void partUpdate(int starti, int startj, int height, int width) {
		for (int i = starti; i < starti + height; ++i)
		{
			for (int j = startj; j < startj + width; ++j)
			{
				const int nearby = countNear(i, j);
				if (nearby >= t)
				{
					int rndk = mrand() % free.size();
					int k = free[rndk];
					swap(field[i][j], field[k / m][k % m]);
					swap4(i*m + j, k);
					free[rndk] = i * m + j;
				}
			}
		}
	}

	void update()
	{
		partUpdate(0, 0, n, m);
	}

	Shelling(unsigned n, unsigned m, float p0, int colours, int t) : n(n), m(m), p0(p0), colours(colours), t(t)
	{
		field.resize(n, vector<int>(m));
		texture.create(m, n);
		pixels = new sf::Uint8[n * m * 4];
		setup();
	}
	Shelling(unsigned n, unsigned m, float p0, vector<float> probs, int t) : n(n), m(m), p0(p0), colours(probs.size()), probs(probs), t(t)
	{
		uniform = false;
		field.resize(n, vector<int>(m));
		texture.create(m, n);
		pixels = new sf::Uint8[n * m * 4];
		setup();
	}
	~Shelling() {
		delete[] pixels;
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

	int colours = 2;
	Shelling model(H / 2, W * 3/8, 0.02, colours , 7); // изображение занимает весь экран по высоте и 3/4 по ширине, но рендерим в 4 раза меньше для производительности

	sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "Schelling segregation", sf::Style::Fullscreen);

	int margin = H / 80, blockOffset = H / 30;
	bool fullscreen = false;
	Button Reset(margin, margin, W / 10, H / 8, "Заново", window);
	Button Exit(margin, H - margin - H/11, W / 16, H / 11, "Выход", window);

	
	Button Colour(margin, margin + H / 8 + 2*blockOffset, W / 10, H / 8, "Кол-во цветов", window); 
	Button Radius(margin, margin + H / 4 + 3*blockOffset , W / 10, H / 8, "Радиус", window);
	Button Tolerance(margin, margin + H * 3 / 8 + 4 * blockOffset, W / 10, H / 8, "Терпимость", window);
	Button Prob0(margin, margin + H /2 + 5 * blockOffset, W / 10, H / 8, "Вероятность 0", window);
	//     0            1       2      3
	// float p0, int colours, int t, int r
	Prob0.id = 0;  Colour.id = 1; Tolerance.id = 2; Radius.id = 3;
	Button Increase(margin + W / 10 + blockOffset, margin + H / 8 + 2 * blockOffset, W / 10, H / 7, "Увеличить", window);
	Button Decrease(margin + W / 10 + blockOffset, margin + H / 8 + H / 7 + 3 * blockOffset, W / 10, H / 7, "Уменьшить", window);

	sf::Text ModelInfo; 
	ModelInfo.setFont(font);
	string Info = model.getInfo();
	ModelInfo.setString(sf::String::fromUtf8(Info.begin(), Info.end()));
	textFormat(ModelInfo, W / 9);
	ModelInfo.setFillColor(sf::Color::Black);
	ModelInfo.setPosition(margin + W / 10 + blockOffset, margin);

	Button* LastLocked = &Colour;

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
		if (Reset.handle()) model.setup();
		if (Exit.handle()) window.close();

		if (Colour.handle()) {
			if (Colour.isLocked())
				Colour.unlock();
			else {
				LastLocked->unlock();
				Colour.lock();
				LastLocked = &Colour;
			}

		}
		if (Radius.handle()) {
			if (Radius.isLocked())
				Radius.unlock();
			else {
				LastLocked->unlock();
				Radius.lock();
				LastLocked = &Radius;
			}
		}
		if (Tolerance.handle()) {
			if (Tolerance.isLocked())
				Tolerance.unlock();
			else {
				LastLocked->unlock();
				Tolerance.lock();
				LastLocked = &Tolerance;
			}
		}
		if (Prob0.handle()) {
			if (Prob0.isLocked())
				Prob0.unlock();
			else {
				LastLocked->unlock();
				Prob0.lock();
				LastLocked = &Prob0;
			}
		}

		if (Increase.handle()) {
			if (LastLocked->isLocked()) model.setParam(LastLocked->id, 1);
		}
		if (Decrease.handle()) {
			if (LastLocked->isLocked()) model.setParam(LastLocked->id, 0);
		}
		Info = model.getInfo();
		ModelInfo.setString(sf::String::fromUtf8(Info.begin(), Info.end()));	
		window.draw(ModelInfo);
		window.display();
	}

	return 0;
}