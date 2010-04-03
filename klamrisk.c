#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>

// These dimensions are in made-up units, where the width of an elevator shaft
// is 60 pixels, and pixels are square.

#define OUTERPOS 80
#define WALLPOS 30
#define SPLATTERPOS 20
#define DOORHEIGHT 100
#define FLOOR (0)		/* origo in middle of screen */
 
#define CIRCLEMAX 32

// Increase for higher resolution
#define CIRCLEDIM 128
#define NPARTICLE 256

#define MAXDOOR 8

typedef enum {
	LEFT,
	RIGHT
} direction_t;

enum {
	T_CIRCLE,
};

struct particle {
	int			x, y;
	int			dx, dy;
	int			ddy;
	int			r;
};

struct shaft {
	int			alive;
	int			animframe;
	direction_t		direction;
	struct particle		particle[NPARTICLE];
};

struct doors {
	int			ypos[MAXDOOR];		// coordinate of floor, disabled if <= -ymax
};

// *************** Globals *************** 
 
int screen_width, screen_height;	// in actual pixels
double ymax;				// in made-up units
SDL_Surface *screen;

struct shaft shaft[4];
struct doors doors[5];

int appearance_timer, rate;

// *************** Setup *************** 

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

	ymax = 640.0 * screen_height / screen_width;

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

// *************** Graphic primitives *************** 

static void fillrect(double x1, double y1, double x2, double y2) {
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glVertex2d(x1, y1);
	glVertex2d(x1, y2);
	glVertex2d(x2, y2);
	glVertex2d(x2, y1);
	glEnd();
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

// *************** Complex drawing operations *************** 

static void draw_doors(struct doors *d) {
	int i;

	// Draw doors on the right side. We're invoked under a flipped transformation in order to draw left doors.

	glColor3d(0, 0, 0);
	fillrect(WALLPOS+2, -ymax, WALLPOS+4, ymax);
	for(i = 0; i < MAXDOOR; i++) {
		if(d->ypos[i] > -ymax) {
			glColor3d(1, 1, 1);
			fillrect(WALLPOS, d->ypos[i] - DOORHEIGHT, OUTERPOS, d->ypos[i]);
			glColor3d(0, 0, 0);
			fillrect(WALLPOS+2, d->ypos[i] - DOORHEIGHT, OUTERPOS, d->ypos[i] - DOORHEIGHT + 2);
			fillrect(WALLPOS+2, d->ypos[i] - 2, OUTERPOS, d->ypos[i]);
			fillrect(WALLPOS+4, d->ypos[i] - DOORHEIGHT, WALLPOS+6, d->ypos[i]);
			fillrect(WALLPOS+8, d->ypos[i] - DOORHEIGHT, WALLPOS+10, d->ypos[i]);
		}
	}
}

static void draw_shaft(struct shaft *shaft, struct doors *left, struct doors *right, int xpos) {
	int i, angle, offset;

	if(shaft->animframe > 10) {
		offset = -2 * (shaft->animframe - 10);
	} else {
		offset = 0;
	}
	glPushMatrix();
		glTranslated(xpos, 0, 0);

		// Draw the lift
		glPushMatrix();
			glColor3d(1, 1, 1);
			fillrect(-OUTERPOS, -ymax, OUTERPOS, ymax);
			glTranslated(0, offset, 0);
			glColor3d(0, 0, 0);
			fillrect(-WALLPOS, FLOOR, WALLPOS, FLOOR + 2);
			fillrect(-WALLPOS, FLOOR - DOORHEIGHT, WALLPOS, FLOOR - DOORHEIGHT + 2);
		glPopMatrix();

		// Draw doors on both sides
		draw_doors(right);
		glPushMatrix();
			glScaled(-1, 1, 1);
			draw_doors(left);
		glPopMatrix();

		// Draw the contents of the lift
		glPushMatrix();
			if(shaft->direction == LEFT) glScaled(-1, 1, 1);
			glTranslated(0, offset, 0);
			glPushMatrix();
				glTranslated(30, FLOOR - 2, 0);
				angle = shaft->animframe * 2;
				if(angle > 30) angle = 30;
				glTranslated(-angle / 16, -angle / 2, 0);
				glRotated(-angle, 0, 0, 1);
				glColor3d(0, 0, 0);
				fillrect(-30, -52, 0, 0);
				glColor3d(1, 1, 1);
				fillrect(-28, -50, -2, -2);
			glPopMatrix();
		glPopMatrix();

		// Draw the red lemonade
		if(shaft->direction == RIGHT) glScaled(-1, 1, 1);
		for(i = 0; i < NPARTICLE; i++) {
			if(shaft->particle[i].r > 0 && shaft->particle[i].y < (ymax + CIRCLEMAX) * 8) {
				draw_circle(shaft->particle[i].x / 8, shaft->particle[i].y / 8, shaft->particle[i].r);
			}
		}
	glPopMatrix();
}

static void drawframe() {
	int i;

	// Set background
	glClearColor(.992, 1, .196, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-640, 640, ymax, -ymax, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//todo
	//glColor3d(.47, .47, .47);
	//fillrect(-82, 0, 82, 480);

	for(i = 0; i < 4; i++) {
		draw_shaft(&shaft[i], &doors[i], &doors[i + 1], 160 * (i - 2) + 80);
	}
}

// *************** Gameplay *************** 

static void splat(struct shaft *shaft) {
	int i;

	for(i = 0; i < NPARTICLE; i++) {
		shaft->particle[i].x = SPLATTERPOS * 8;
		shaft->particle[i].y = (FLOOR - 45) * 8;
		shaft->particle[i].dx = (rand() % 64) - 32;
		shaft->particle[i].dy = (rand() % 64) - 48;
		shaft->particle[i].r = rand() % (CIRCLEMAX / 2);
		shaft->particle[i].ddy = 2;
	}
}

static void doors_physics(struct doors *d) {
	int i;

	for(i = 0; i < MAXDOOR; i++) {
		if(d->ypos[i] > -ymax) {
			d->ypos[i] -= 2;
		}
	}
}

static void shaft_physics(struct shaft *shaft, struct doors *left, struct doors *right) {
	int i;

	if(shaft->alive) {
		struct doors *dangerous = (shaft->direction == LEFT)? left : right;

		for(i = 0; i < MAXDOOR; i++) {
			if(dangerous->ypos[i] > FLOOR - 3 && dangerous->ypos[i] < FLOOR) {
				shaft->alive = 0;
			}
		}
	} else {
		shaft->animframe++;
		if(shaft->animframe == 15) {
			splat(shaft);
		}
	}
	for(i = 0; i < NPARTICLE; i++) {
		if(shaft->particle[i].r > 0 && shaft->particle[i].y < (ymax + CIRCLEMAX) * 8) {
			shaft->particle[i].x += shaft->particle[i].dx;
			if(shaft->particle[i].x > WALLPOS * 8) {
				shaft->particle[i].x -= shaft->particle[i].dx;
				shaft->particle[i].dx *= -.1;
				if(rand() % 3) {
					shaft->particle[i].ddy = 0;
					if(shaft->particle[i].dy < 0) shaft->particle[i].dy = 0;
					shaft->particle[i].dx = 0;
				}
			}
			if(shaft->particle[i].x < -WALLPOS * 8) {
				shaft->particle[i].x -= shaft->particle[i].dx;
				shaft->particle[i].dx *= -.1;
				if(rand() % 3) {
					shaft->particle[i].ddy = 0;
					if(shaft->particle[i].dy < 0) shaft->particle[i].dy = 0;
					shaft->particle[i].dx = 0;
				}
			}
			shaft->particle[i].y += shaft->particle[i].dy;
			shaft->particle[i].dy += shaft->particle[i].ddy;
			if(shaft->particle[i].r && !(rand() % 5)) shaft->particle[i].r--;
		}
	}
}

static void resetshaft(struct shaft *shaft) {
	shaft->alive = 1;
	shaft->animframe = 0;
	shaft->direction = LEFT;
}

static void resetdoors(struct doors *d) {
	int i;

	for(i = 0; i < MAXDOOR; i++) {
		d->ypos[i] = -ymax;
	}
}

static void flip(struct shaft *shaft) {
	if(shaft->alive) {
		shaft->direction ^= (LEFT ^ RIGHT);
	}
}

static void newgame() {
	int i;

	for(i = 0; i < 4; i++) {
		resetshaft(&shaft[i]);
	}
	for(i = 0; i < 5; i++) {
		resetdoors(&doors[i]);
	}
	rate = 50;
	appearance_timer = rate;
}

static void add_doors() {
	if(rate) {
		if(appearance_timer) {
			appearance_timer--;
		} else {
			int which = rand() % 5;
			int i;

			for(i = 0; i < MAXDOOR; i++) {
				if(doors[which].ypos[i] <= -ymax) {
					doors[which].ypos[i] = ymax + DOORHEIGHT;
					break;
				}
			}
			appearance_timer = rate;
		}
	}
}

int main(int argc, char *argv[])
{
	int i;
	int running = 1;
	Uint32 lasttick;

	init_video(0); // SDL_FULLSCREEN;
	precalc();
	newgame();

	lasttick = SDL_GetTicks();
	while (running) {
		SDL_Event event;
		Uint32 now = SDL_GetTicks();
		while(now - lasttick > 20) {
			lasttick += 20;
			for(i = 0; i < 5; i++) {
				doors_physics(&doors[i]);
			}
			for(i = 0; i < 4; i++) {
				shaft_physics(&shaft[i], &doors[i], &doors[i + 1]);
			}
			add_doors();
		}

		while ( SDL_PollEvent(&event) ) {
			switch (event.type) {
				case SDL_QUIT:
					running = 0;
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym) {
						case SDLK_1:
							flip(&shaft[0]);
							break;
						case SDLK_2:
							flip(&shaft[1]);
							break;
						case SDLK_3:
							flip(&shaft[2]);
							break;
						case SDLK_4:
							flip(&shaft[3]);
							break;
						case SDLK_SPACE:
							newgame();
							break;
						case SDLK_ESCAPE:
							running = 0;
							break;
					}
					break;
			}
		}

		drawframe();

		// Flip
		SDL_GL_SwapBuffers();
	}
	return 0;
}
