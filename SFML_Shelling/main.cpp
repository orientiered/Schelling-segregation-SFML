#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <set>
#include <time.h>
#include <random>
#include <SFML/Graphics/Color.hpp>
#include <thread>
#include <mutex>
using namespace std;
mt19937 mrand(time(nullptr));
mutex mtx;
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

class Shelling
{
public:
	unsigned n;
	unsigned m;
	vector<vector<int>> field;
	float p0 = 0.2; //вероятность пустых клеток
	int colors = 0; // общее количество цветов (не считая пустого)
	bool uniform = true; // равномерное распределение цветов
	vector<float> probs; // если неравномерное
	const float maxr = float(2 << 16);
	int t = 3; // терпимость, макс. количество чужих вокруг
	sf::Uint8* pixels;
	sf::Texture texture;

	vector<int> free; // массив пустых клеток

	//меняем блоки из 4 Uint8 в pixels
	void swap4(int i, int j, int k, int t) {
		swap(pixels[4 * (i * m + j)], pixels[4 * (k * m + t)]);
		swap(pixels[4 * (i * m + j) + 1], pixels[4 * (k * m + t) + 1]);
		swap(pixels[4 * (i * m + j) + 2], pixels[4 * (k * m + t) + 2]);
		swap(pixels[4 * (i * m + j) + 3], pixels[4 * (k * m + t) + 3]);
	}; 
	void setup() {

		for (int i = 0; i < n; ++i) {
			for (int j = 0; j < m; ++j) {

				float rt = float(mrand() % int(maxr)) / maxr;

				if (rt < p0)
				{
					field[i][j] = 0;
					free.push_back(i * m + j);
				}
				else if (uniform) {
					field[i][j] = (rt-p0)/(1-p0) * colors;
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
				quad col = hsv(field[i][j] * 360 / colors, 0.95 * (field[i][j] != 0), 0.95 * (field[i][j] != 0));
				col.set(pixels, 4 * (i * m + j));
			}
		}
	}

	void draw(sf::RenderWindow& window)
	{
		static sf::Sprite sprite(texture);
		texture.update(pixels);
		window.draw(sprite);
	}

	int countNear(int x, int y, int r = 2)
	{
		int w = m, h = n;
		if (field[x][y] == 0) return 0;
		vector<int> colorCount(colors+1);
		int sum = 0;
		for (int i = x - r; i < x + r; i++)
		{
			for (int j = y - r; j < y + r; j++)
			{
				colorCount[field[(i + h) % h][(j + w) % w]]++;
				sum += (field[(i + h) % h][(j + w) % w] > 0);
			}
		}
		return sum - colorCount[field[x][y] - 1];
	}

	void partUpdate(int starti, int startj, int height, int width) {
		//int perms = 0;
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
					swap4(i, j, k / m, k % m);
					free[rndk] = i * m + j;
					//perms++;
				}
			}
		}
	}

	void update()
	{
		partUpdate(0, 0, n, m);
	}

	Shelling(unsigned n, unsigned m, float p0, int colors, int t) : n(n), m(m), p0(p0), colors(colors), t(t)
	{
		field.resize(n, vector<int>(m));
		texture.create(m, n);
		pixels = new sf::Uint8[n * m * 4];
		setup();
	}
	Shelling(unsigned n, unsigned m, float p0, vector<float> probs, int t) : n(n), m(m), p0(p0), colors(probs.size()), probs(probs), t(t)
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





int main()
{
	const unsigned int W = 1280;
	const unsigned int H = 720;
	Shelling model(H, W, 0.01, {0.3, 0.2, 0.49}, 9);
	//Shelling model(H, W, 0.01, {0.33, 0.32, 0.6}, 5);
	sf::RenderWindow window(sf::VideoMode(W, H), "Shelling segregation");

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}

		window.clear();
		model.update();
		model.draw(window);
		window.display();
	}

	return 0;
}