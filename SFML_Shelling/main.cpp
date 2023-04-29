#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <set>
#include <time.h>
#include <random>
#include <SFML/Graphics/Color.hpp>
#include <thread>
using namespace std;
mt19937 mrand(time(nullptr));

struct quad {
	uint8_t r, g, b, a;
	quad(int r, int g, int b) : r(r), g(g), b(b), a(255) {}
};

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
	case 6: return { (int) val * 255, (int) t * 255, (int) p * 255 };
	case 1: return { (int) q * 255, (int) val * 255, (int) p * 255 };
	case 2: return { (int) p * 255, (int) val * 255, (int) t * 255 };
	case 3: return { (int) p * 255, (int) q * 255, (int) val * 255 };
	case 4: return { (int) t * 255, (int) p * 255, (int) val * 255 };
	case 5: return { (int) val * 255, (int) p * 255, (int) q * 255 };
	}
}

class Shelling
{
public:
	unsigned n;
	unsigned m;
	vector<vector<int>> field;
	float p0 = 0.2, p1 = 0.4;
	const float maxr = float(2 << 16);
	int t = 3;
	sf::Uint8* pixels;
	sf::Texture texture;

	vector<int> free;
	void swap4(int i, int j, int k, int t) {
		swap(pixels[4 * (i * m + j)], pixels[4 * (k * m + t)]);
		swap(pixels[4 * (i * m + j)+1], pixels[4 * (k * m + t)+1]);
		swap(pixels[4 * (i * m + j)+2], pixels[4 * (k * m + t)+2]);
		swap(pixels[4 * (i * m + j)+3], pixels[4 * (k * m + t)+3]);
	};//меняем блоки из 4 Uint8 в pixels 
	void setup()
	{
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < m; ++j)
			{
				int rt = mrand() % int(maxr);
				if (rt < maxr * p0)
				{
					field[i][j] = 0;
					free.push_back(i * m + j);
				}
				else if (rt >= maxr * p0 && rt < maxr * (p0 + p1) ) {
					field[i][j] = 1;
				}
				else {
					field[i][j] = 2;
				}
				//quad c = hsv(field[i][j] * 360 / 2, 1, (field[i][j] ? 1 : 0));
				pixels[4 * (i * m + j)] = field[i][j] == 1 ? 255 : 0 ;
				pixels[4 * (i * m + j)+1] = field[i][j] == 2 ? 255 : 0;
				pixels[4 * (i * m + j)+2] = 0;
				pixels[4 * (i * m + j)+3] = 255;
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
		int c1 = 0, c2 = 0;
		for (int i = max(0, x - r); i < min(int(n), x + r); i++)
		{
			for (int j = max(0, y - r); j < min(int(m), y + r); j++)
			{
				c1 += (field[i][j] == 1);
				c2 += (field[i][j] == 2);
			}
		}
		switch (field[x][y])
		{
		case 1:
			return c2;
		case 2:
			return c1;
		}
		return 0;
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

	int update()
	{
		int perms = 0;
		thread t1(&Shelling::partUpdate, this, 0, 0, n / 2, m / 2);

		thread t2(&Shelling::partUpdate, this, n / 2, 0, n / 2, m / 2);

		thread t3(&Shelling::partUpdate, this, 0, m / 2, n / 2, m / 2);

		thread t4(&Shelling::partUpdate, this, n / 2, m / 2, n / 2, m / 2);
		t1.join();
		t2.join();
		t3.join();
		t4.join();
		return 1;
	}

	Shelling(unsigned n, unsigned m, float p0, float p1, float t) : n(n), m(m), p0(p0), p1(p1), t(t)
	{
		field.resize(n, vector<int>(m));
		texture.create(m, n);
		pixels = new sf::Uint8[n * m * 4];
		setup();
	}

	~Shelling() {
		delete[] pixels;
	}
};

// hue: 0-360°; sat: 0.f-1.f; val: 0.f-1.f



int main()
{
	const unsigned int W = 1280;
	const unsigned int H = 720; 
	Shelling model(H, W, 0.1, 0.5, 4);
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