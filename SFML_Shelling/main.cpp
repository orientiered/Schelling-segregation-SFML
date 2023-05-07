#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <time.h>
#include <random>
#include <thread>
#include <chrono>
#include <format>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Keyboard.hpp>

using namespace std;

mt19937 mrand(time(nullptr));

#define kb sf::Keyboard::

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

class EventSeparator { // класс для контроля работы разных потоков над одним объектом
private:
	deque<int> eventQueue;
public:
	void startEvent(int thread = 0) {
		eventQueue.push_back(thread);
		while (eventQueue.front() != thread) {
			this_thread::sleep_for(chrono::milliseconds(5));
		}
	}
	void endEvent() {
		eventQueue.pop_front();
	}

};

class Shelling: public EventSeparator
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
		return "Current parameters: p0 = " + format("{:.2f}", p0) + " colours = " + to_string(colours) + " t = " + to_string(t) + " r = " + to_string(r);
	}
	// float p0, int colours, int t, int r
	void setParam(int paramId, bool inc) {
		startEvent(1);
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
		setup(-1);
		endEvent();
	}

	void setup(int thread = 0) {
		
		if (thread >= 0) startEvent(thread); //если номер потока меньше 0, то считаем, что о синхронизации позаботились в другой функции

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
		
		if (thread >= 0) endEvent();
	}

	void draw(sf::RenderWindow& window)
	{
		static sf::Sprite sprite(texture);
		texture.update(pixels);
		sprite.setScale(sf::Vector2f(float(window.getSize().x)/m, float(window.getSize().y)/n));
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

	void update(int thread = 0)
	{
		startEvent(thread);

		partUpdate(0, 0, n, m);
		
		endEvent();
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



void InputHandler(Shelling& model) {
	bool pressedUp = false;
	bool pressedDown = false;
	bool pressedLeft = false;
	bool pressedRight = false;
	bool pressedSpace = false;

	int paramId = 1;
	//     0            1       2      3
	// float p0, int colours, int t, int r
	vector<string> fieldNames = { "Empty probability", "Number of colours", "Tolerance (max number of enemies)", "Radius of enemies checking" };

	cout << "Editable setting: " << fieldNames[paramId] << "\n";
	while (true) {
		if (kb isKeyPressed(kb Key::Space)) pressedSpace = true;
		else if (pressedSpace) {
			pressedSpace = false;
			model.setup(1);
		}
		if (kb isKeyPressed(kb Key::Up)) {
			pressedUp = true;
		}
		else if (pressedUp) {
			model.setParam(paramId, 1);
			pressedUp = false;
			cout << model.getInfo() << "\n";
		}
		if (kb isKeyPressed(kb Key::Down)) {
			pressedDown = true;
		}
		else if (pressedDown) {
			model.setParam(paramId, 0);
			pressedDown = false;
			cout << model.getInfo() << "\n";
		}

		if (kb isKeyPressed(kb Key::Left)) {
			pressedLeft = true;
		}
		else if (pressedLeft) {
			paramId = (paramId - 1 + fieldNames.size()) % fieldNames.size();
			pressedLeft = false;
			cout << "Editable setting: " << fieldNames[paramId] << "\n";
		}
		if (kb isKeyPressed(kb Key::Right)) {
			pressedRight = true;
		}
		else if (pressedRight) {
			paramId = (paramId + 1) % fieldNames.size();
			pressedRight = false;
			cout << "Editable setting: " << fieldNames[paramId] << "\n";
		}
	}
	

}

int main() {
	unsigned int W = sf::VideoMode::getDesktopMode().width * 2 / 3;
	unsigned int H = sf::VideoMode::getDesktopMode().height * 2 / 3;

	int colours = 2;
	Shelling model(H, W, 0.02, colours , 7);

	cout << model.getInfo() << "\n";
	thread ISystem(InputHandler, ref(model));
	ISystem.detach();
	sf::RenderWindow window(sf::VideoMode(W, H), "Schelling segregation");
	
	bool fullscreen = false;

	while (window.isOpen())
	{
		

		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::F12)
			{
				fullscreen = !fullscreen;
				if (fullscreen) {
					window.create(sf::VideoMode::getDesktopMode(), "Schelling Segregation", sf::Style::None);
				}
				else {
					window.create(sf::VideoMode(W, H), "Schelling segregation");
				}
			}
		}

		window.clear();
		model.update();
		model.draw(window);
		window.display();
	}

	return 0;
}