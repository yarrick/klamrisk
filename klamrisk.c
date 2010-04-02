#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>

int screen_width, screen_height;

// Legacy, not related to actual dimensions:
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define WALLPOS 30
#define SPLATTERPOS 20

#define CIRCLEMAX 32
#define CIRCLEDIM 128
#define NPARTICLE 256

#define DOORHEIGHT 80
#define FLOOR 170
 
SDL_Surface *screen;

enum dir { LEFT, RIGHT };

struct particle {
	int	x, y;
	int	dx, dy;
	int	ddy;
	int	r;
} particle[NPARTICLE];
 
enum {
	T_CIRCLE,
};

static int init_video(Uint32 flags) {
	SDL_Rect **modes;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
		return 0;
	}
	atexit(SDL_Quit); 

	modes = SDL_ListModes(0, SDL_FULLSCREEN | SDL_HWSURFACE | SDL_OPENGL);
	if(modes == 0 || modes == (SDL_Rect **) -1) {
		screen_width = 640;
		screen_height = 480;
	} else {
		screen_width = modes[0]->w;
		screen_height = modes[0]->h;
	}

	screen = SDL_SetVideoMode(screen_width, screen_height, 0, flags | SDL_HWSURFACE | SDL_OPENGL);
	if (!screen) {
		fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
		return 0;
	}

	SDL_ShowCursor(0);

	return 1;
}

static void precalc() {
	int x, y;
	uint8_t circlebuf[CIRCLEDIM][CIRCLEDIM];

	for(y = 0; y < CIRCLEDIM; y++) {
		for(x = 0; x < CIRCLEDIM; x++) {
			int r = floor(sqrt(
				(y - CIRCLEDIM/2) * (y - CIRCLEDIM/2) +
				(x - CIRCLEDIM/2) * (x - CIRCLEDIM/2)));
			circlebuf[y][x] = (r < CIRCLEDIM/2)? 0xff : 0x00;
		}
	}

	glBindTexture(GL_TEXTURE_2D, T_CIRCLE);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_ALPHA, CIRCLEDIM, CIRCLEDIM, GL_ALPHA, GL_UNSIGNED_BYTE, circlebuf);
}

static void splatter(int x, int y) {
	int i;

	for(i = 0; i < NPARTICLE; i++) {
		particle[i].x = x * 8;
		particle[i].y = y * 8;
		particle[i].dx = (rand() % 64) - 32;
		particle[i].dy = (rand() % 64) - 48;
		particle[i].r = rand() % (CIRCLEMAX / 2);
		particle[i].ddy = 2;
	}
}

static void fillrect(double x1, double y1, double x2, double y2) {
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glVertex2d(x1, y1);
	glVertex2d(x1, y2);
	glVertex2d(x2, y2);
	glVertex2d(x2, y1);
	glEnd();
}

static void draw_door(int y, enum dir side) {
	if(side == RIGHT) {
		glScaled(-1, 1, 1);
	}
	glColor3d(1, 1, 1);
	fillrect(-WALLPOS-2, y - DOORHEIGHT, -WALLPOS+1, y);
	glColor3d(0, 0, 0);
	fillrect(-WALLPOS-50, y, -WALLPOS-1, y + 2);
	fillrect(-WALLPOS-50, y - DOORHEIGHT - 2, -WALLPOS-1, y - DOORHEIGHT);
	fillrect(-WALLPOS-10, y - DOORHEIGHT, -WALLPOS-8, y);
	fillrect(-WALLPOS-6, y - DOORHEIGHT, -WALLPOS-4, y);
	if(side == RIGHT) {
		glScaled(-1, 1, 1);
	}
}

static void draw_circle(int xpos, int ypos, int r) {
	int x, y;

	glColor3d(1, 0, 0);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_QUADS);
	glTexCoord2d(0, 0);
	glVertex2d(xpos - r, ypos - r);
	glTexCoord2d(0, 1);
	glVertex2d(xpos - r, ypos + r);
	glTexCoord2d(1, 1);
	glVertex2d(xpos + r, ypos + r);
	glTexCoord2d(1, 0);
	glVertex2d(xpos + r, ypos - r);
	glEnd();
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

static void draw_lift(enum dir side)
{
	glColor3d(0, 0, 0);
	fillrect(-WALLPOS, FLOOR, WALLPOS, FLOOR + 2);
	fillrect(-WALLPOS, FLOOR - DOORHEIGHT, WALLPOS, FLOOR - DOORHEIGHT + 2);

	//SDL_Rect tunna = { side == LEFT ? LEFTSIDE : RIGHTSIDE - 25, FLOOR-40, 25, 40};
	//SDL_FillRect(screen, &tunna, BLACK);
}

int main(int argc, char *argv[])
{
	int i;

	init_video(0); // SDL_FULLSCREEN);
	precalc();

	int running = 1;
	int alive = 0;
	int y = SCREEN_HEIGHT + DOORHEIGHT + 10;
	enum dir door = LEFT;
	enum dir side = LEFT;
	Uint32 lasttick = SDL_GetTicks();

	while (running) {
		SDL_Event event;
		Uint32 now = SDL_GetTicks();
		while(now - lasttick > 20) {
			lasttick += 20;

			// Update physics.
			if(alive) {
				y -= 2;
				if (y > -DOORHEIGHT) {
					if (y >= FLOOR && y <= FLOOR +3 && side == door) {
						// DIIEEEEEEE!!!
						alive = 0;
						splatter(side == LEFT ? SPLATTERPOS : -SPLATTERPOS, FLOOR - 45);
					}
				} else {
					// Start new door
					y = SCREEN_HEIGHT + DOORHEIGHT + 5;
					door = (door == LEFT ? RIGHT : LEFT);
				}
			}
			for(i = 0; i < NPARTICLE; i++) {
				if(particle[i].r > 0 && particle[i].y < (SCREEN_HEIGHT + CIRCLEMAX) * 8) {
					particle[i].x += particle[i].dx;
					if(particle[i].x > WALLPOS * 8) {
						particle[i].x -= particle[i].dx;
						particle[i].dx *= -.1;
						if(rand() % 3) {
							particle[i].ddy = 0;
							if(particle[i].dy < 0) particle[i].dy = 0;
							particle[i].dx = 0;
						}
					}
					if(particle[i].x < -WALLPOS * 8) {
						particle[i].x -= particle[i].dx;
						particle[i].dx *= -.1;
						if(rand() % 3) {
							particle[i].ddy = 0;
							if(particle[i].dy < 0) particle[i].dy = 0;
							particle[i].dx = 0;
						}
					}
					particle[i].y += particle[i].dy;
					particle[i].dy += particle[i].ddy;
					if(particle[i].r && !(rand() % 5)) particle[i].r--;
				}
			}
		}

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
							if (alive)
								side = (side == LEFT ? RIGHT : LEFT);
							break;
						case SDLK_F1: /* new game */
							if (!alive) {
								alive = 1;
								y = SCREEN_HEIGHT + DOORHEIGHT + 5;
							}
							break;
						case SDLK_x:
							splatter(-10, SCREEN_HEIGHT / 2);
							break;
						case SDLK_ESCAPE:
							running = 0;
							break;
					}
					break;
			}
		}

		// Set background
		glClearColor(.992, 1, .196, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(
			-SCREEN_WIDTH/2 * ((double) screen_width / screen_height) / ((double) SCREEN_WIDTH / SCREEN_HEIGHT),
			SCREEN_WIDTH/2 * ((double) screen_width / screen_height) / ((double) SCREEN_WIDTH / SCREEN_HEIGHT),
			SCREEN_HEIGHT,
			0,
			-1,
			1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glColor3d(.47, .47, .47);
		fillrect(-82, 0, 82, 480);
		glColor3d(1, 1, 1);
		fillrect(-80, 0, 80, 480);
		glColor3d(0, 0, 0);
		fillrect(-WALLPOS-2, 0, -WALLPOS, 480);
		fillrect(WALLPOS, 0, WALLPOS+2, 480);

		for(i = 0; i < NPARTICLE; i++) {
			if(particle[i].r > 0 && particle[i].y < (SCREEN_HEIGHT + CIRCLEMAX) * 8) {
				draw_circle(particle[i].x / 8, particle[i].y / 8, particle[i].r);
			}
		}

		glColor3d(1, 1, 1);
		fillrect(-80, 0, -WALLPOS-2, 480);
		fillrect(WALLPOS+2, 0, 80, 480);

		draw_door(y, door);
		draw_lift(side);

		// Flip
		SDL_GL_SwapBuffers();
	}
	return 0;
}
