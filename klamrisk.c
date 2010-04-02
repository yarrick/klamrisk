#include <math.h>
#include <SDL/SDL.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define COLORKEY 255, 0, 255 //Your Transparent colour

#define KLAMRISK_BG 0xfdff32
#define WHITE 0xffffff
#define GREY 0x777777
#define BLACK 0x0

#define LEFTSIDE 290
#define RIGHTSIDE 350

#define CIRCLEMAX 32
#define NPARTICLE 256

#define DOORHEIGHT 80
 
SDL_Surface *screen;

// White elevator shaft
SDL_Rect outershaft = { 238, 0, 164, 480 };
SDL_Rect shaft = { 240, 0, 160, 480 };
SDL_Rect leftwall = {LEFTSIDE, 0, 1, 480};
SDL_Rect rightwall = {RIGHTSIDE, 0, 1, 480};

enum door { LEFT, RIGHT };

char circlebuf[CIRCLEMAX][CIRCLEMAX];
struct particle {
	int	x, y;
	int	dx, dy;
	int	r;
} particle[NPARTICLE];
 
static int init_video(Uint32 flags) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
		return 0;
	}
	atexit(SDL_Quit); 

	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 24, flags);
	if (!screen) {
		fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
		return 0;
	}
	return 1;
}

static void precalc() {
	int x, y;

	for(y = 0; y < CIRCLEMAX; y++) {
		for(x = 0; x < CIRCLEMAX; x++) {
			int r = floor(sqrt(
				(y - CIRCLEMAX/2) * (y - CIRCLEMAX/2) +
				(x - CIRCLEMAX/2) * (x - CIRCLEMAX/2)));
			circlebuf[y][x] = r;
		}
	}
}

static void splatter(int x, int y) {
	int i;

	for(i = 0; i < NPARTICLE; i++) {
		particle[i].x = x * 8;
		particle[i].y = y * 8;
		particle[i].dx = (rand() % 64) - 32;
		particle[i].dy = (rand() % 64) - 32;
		particle[i].r = rand() % (CIRCLEMAX / 2);
	}
}

static void draw_door(int y, enum door side)
{
	SDL_Rect floor = { side == LEFT ? LEFTSIDE - 50 : RIGHTSIDE, y, 50, 1};
	SDL_Rect roof = { side == LEFT ? LEFTSIDE - 50 : RIGHTSIDE, y - DOORHEIGHT, side==LEFT ? 51:50, 1};
	SDL_Rect wall = { side == LEFT ? LEFTSIDE : RIGHTSIDE, y - DOORHEIGHT, 1, DOORHEIGHT};
	SDL_Rect door = { side == LEFT ? LEFTSIDE - 8: RIGHTSIDE + 3, y - DOORHEIGHT, 5, DOORHEIGHT};
	SDL_Rect doorinside = { side == LEFT ? LEFTSIDE - 7: RIGHTSIDE + 4, y - DOORHEIGHT +2, 3, DOORHEIGHT-3};

	// Clear wall
	SDL_FillRect(screen, &wall, WHITE);

	SDL_FillRect(screen, &floor, BLACK);
	SDL_FillRect(screen, &roof, BLACK);
	SDL_FillRect(screen, &door, BLACK);
	SDL_FillRect(screen, &doorinside, WHITE);
}

static void draw_circle(int xpos, int ypos, int r) {
	int x, y;

	for(y = -r; y < r; y++) {
		if(ypos + y >= 0 && ypos + y < SCREEN_HEIGHT) {
			for(x = -r; x < r; x++) {
				if(xpos + x >= shaft.x && xpos + x < shaft.x + shaft.w) {
					if(circlebuf[y + CIRCLEMAX/2][x + CIRCLEMAX/2] < r) {
						uint8_t *pix = (uint8_t *) screen->pixels + screen->pitch * (ypos + y) + 3 * (xpos + x);
						pix[0] = 0x00;
						pix[1] = 0x00;
						pix[2] = 0xff;
					}
				}
			}
		}
	}
}

int main(int argc, char *argv[])
{
	int i;

	init_video(SDL_DOUBLEBUF);// |SDL_FULLSCREEN
	precalc();

	int running = 1;
	enum door side = LEFT;

	while (running) {
		SDL_Event event;
		while ( SDL_PollEvent(&event) ) {
			switch (event.type) {
				case SDL_QUIT:
					running = 0;
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym) {
						case SDLK_LEFT:
							side = LEFT;
							break;
						case SDLK_RIGHT:
							side = RIGHT;
							break;
						case SDLK_x:
							splatter(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
							break;
						case SDLK_ESCAPE:
							running = 0;
							break;
					}
					break;
			}
		}

		// Set background
		SDL_FillRect(screen, NULL, KLAMRISK_BG);

		SDL_FillRect(screen, &outershaft, GREY);
		SDL_FillRect(screen, &shaft, WHITE);
		SDL_FillRect(screen, &leftwall, BLACK);
		SDL_FillRect(screen, &rightwall, BLACK);

		int y = 480 - SDL_GetTicks() / 50;
		if (y > -DOORHEIGHT) {
			draw_door(y, side);
		}

		for(i = 0; i < NPARTICLE; i++) {
			if(particle[i].r > 0 && particle[i].y < (SCREEN_HEIGHT + CIRCLEMAX) * 8) {
				draw_circle(particle[i].x / 8, particle[i].y / 8, particle[i].r);
				particle[i].x += particle[i].dx;
				if(particle[i].x > (shaft.x + shaft.w) * 8) {
					particle[i].x -= particle[i].dx;
					particle[i].dx *= -1;
				}
				if(particle[i].x < shaft.x * 8) {
					particle[i].x -= particle[i].dx;
					particle[i].dx *= -1;
				}
				particle[i].y += particle[i].dy;
				particle[i].dy += 2;
			}
		}

		// Flip
		SDL_Flip(screen);
	}
	return 0;
}
