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

#define DOORHEIGHT 80
#define FLOOR 170
 
SDL_Surface *screen;

// White elevator shaft
SDL_Rect outershaft = { 238, 0, 164, SCREEN_HEIGHT };
SDL_Rect shaft = { 240, 0, 160, SCREEN_HEIGHT };
SDL_Rect leftwall = {LEFTSIDE, 0, 1, SCREEN_HEIGHT};
SDL_Rect rightwall = {RIGHTSIDE, 0, 1, SCREEN_HEIGHT};

enum dir { LEFT, RIGHT };
 
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

static void draw_door(int y, enum dir side)
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

static void draw_lift(enum dir side)
{
	SDL_Rect floor = { LEFTSIDE + 1, FLOOR, 60, 3};
	SDL_Rect floorinside = { LEFTSIDE + 2, FLOOR+1, 57, 1};
	SDL_Rect tunna = { side == LEFT ? LEFTSIDE : RIGHTSIDE - 25, FLOOR-40, 25, 40};

	SDL_FillRect(screen, &floor, BLACK);
	SDL_FillRect(screen, &floorinside, WHITE);
	SDL_FillRect(screen, &tunna, BLACK);
}

int main(int argc, char *argv[])
{
	init_video(SDL_DOUBLEBUF);// |SDL_FULLSCREEN

	int running = 1;
	int alive = 0;
	double y = SCREEN_HEIGHT;
	double divisor = 10;
	int lasttick = SDL_GetTicks();
	enum dir door = LEFT;
	enum dir side = LEFT;

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
							door = LEFT;
							break;
						case SDLK_RIGHT:
							door = RIGHT;
							break;
						case SDLK_SPACE:
							side = (side == LEFT ? RIGHT : LEFT);
							break;
						case SDLK_F1: /* new game */
							if (!alive) {
								alive = 1;
								y = SCREEN_HEIGHT;
								lasttick = SDL_GetTicks();
							}
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

		if (alive) {
			int tick = SDL_GetTicks();
			double move = tick - lasttick;
			y -= move / divisor;
			lasttick = tick;
		}
		if (y > -DOORHEIGHT) {
			int height = (int) y;
			draw_door(height, door);
			if (height >= FLOOR && height <= FLOOR +3 && side == door) {
				alive = 0;
			}
		} else {
			// Start new door
			y = SCREEN_HEIGHT + DOORHEIGHT + 5;
			door = (door == LEFT ? RIGHT : LEFT);
		}

		draw_lift(side);

		// Flip
		SDL_Flip(screen);
	}
	return 0;
}
