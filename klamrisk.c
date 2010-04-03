#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>
#include <SDL/SDL_ttf.h>

// These dimensions are in made-up units, where the width of an elevator shaft
// is 60 pixels, and pixels are square.

#define OUTERPOS 80
#define WALLPOS 30
#define SPLATTERPOS 20
#define DOORHEIGHT 95
#define FLOOR (0)		/* origo in middle of screen */
 
#define CIRCLEMAX 32

// Increase for higher resolution
#define CIRCLEDIM 128
#define NPARTICLE 256
#define SOUNDFREQ 44100

#define MAXDOOR 8

typedef enum {
	LEFT,
	RIGHT
} direction_t;

enum {
	T_CIRCLE,
	T_EXCLAM,
	T_KRYO,
	T_PRESENTS,
	T_KLAMRISK,
	T_HERO,
	T_LIVECODED,
	T_MENU,
	T_TRAINER,
	T_INSTRUCT1,
	T_INSTRUCT2,
	T_INSTRUCT3,
	T_1,
	T_2,
	T_3,
	T_4,
	NTEXTURE
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
	int			grace;
	direction_t		direction;
	struct particle		particle[NPARTICLE];
};

struct doors {
	int			ypos[MAXDOOR];		// coordinate of floor, disabled if <= -ymax
};

struct oscillator {
	uint16_t		freq, phase;
	int			volume;			// [0, 255]
};

int16_t synthesize();

int texture[NTEXTURE];
double texturesize[NTEXTURE][2];

// *************** Globals *************** 
 
int screen_width, screen_height;	// in actual pixels
double ymax;				// in made-up units
SDL_Surface *screen;

int nbr_shafts;
int nbr_doors;
struct shaft shaft[10];
struct doors doors[10];

int appearance_timer, rate, playing, speed;

Uint32 menutime;
uint16_t freqtbl[64];
volatile struct oscillator osc[2];

TTF_Font *font;

#ifdef WIN32
extern char binary_Allerta_allerta_medium_ttf_start;
extern char binary_Allerta_allerta_medium_ttf_end;
#else
extern char _binary_Allerta_allerta_medium_ttf_start;
extern char _binary_Allerta_allerta_medium_ttf_end;
#endif

// *************** Setup *************** 

static void cback(void *userdata, Uint8 *stream, int len) {
	int16_t *buf = (int16_t *) stream;
	int i, samples = len / sizeof(int16_t);

	for(i = 0; i < samples; i++) {
		buf[i] = synthesize();
	}
}

static int init_sdl() {
	int i;
	SDL_Rect **modes;
	SDL_AudioSpec spec = {
		.freq = SOUNDFREQ,
		.channels = 1,
		.format = AUDIO_S16,
		.samples = 512,
		.callback = cback
	};

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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

	screen = SDL_SetVideoMode(screen_width, screen_height, 0, 
		SDL_HWSURFACE | SDL_OPENGL | SDL_FULLSCREEN);
	if (!screen) {
		fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
		return 0;
	}

	if(SDL_OpenAudio(&spec, 0)) {
		fprintf(stderr, "Unable to set audio mode: %s\n", SDL_GetError());
		return 0;
	}

	glGenTextures(NTEXTURE, texture);

	ymax = 640.0 * screen_height / screen_width;

	for(i = 0; i < 64; i++) {
		freqtbl[i] = (uint16_t) round(440.0 * 65536.0 / SOUNDFREQ * pow(pow(2, 1.0/12), i - 39));
	}

	SDL_PauseAudio(0);
	SDL_ShowCursor(0);

	return 1;
}

static void maketext(int tid, char *text) {
	SDL_Color color = {255, 255, 255};
	SDL_Surface *initial;
	SDL_Surface *intermediary;
	SDL_Rect rect;
	double w, h;

	/* Use SDL_TTF to render our text */
	initial = TTF_RenderText_Blended(font, text, color);

	/* Convert the rendered text to a known format */
	w = initial->w;
	h = initial->h;

	texturesize[tid][0] = w;
	texturesize[tid][1] = h;

	intermediary = SDL_CreateRGBSurface(0, w, h, 32, 
			0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000);

	SDL_BlitSurface(initial, 0, intermediary, 0);

	/* Tell GL about our new texture */
	glBindTexture(GL_TEXTURE_2D, texture[tid]);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, w, h, GL_RGBA, GL_UNSIGNED_BYTE, intermediary->pixels);

	/* GL_NEAREST looks horrible, if scaled... */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	

	/* Bad things happen if we delete the texture before it finishes */
	//glFinish();

	/* Clean up */
	SDL_FreeSurface(initial);
	SDL_FreeSurface(intermediary);
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

	glBindTexture(GL_TEXTURE_2D, texture[T_CIRCLE]);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_ALPHA, CIRCLEDIM, CIRCLEDIM, GL_ALPHA, GL_UNSIGNED_BYTE, circlebuf);

	maketext(T_EXCLAM, "!");
	maketext(T_KRYO, "kryo");
	maketext(T_PRESENTS, "presents");
	maketext(T_KLAMRISK, "KLAMRISK");
	maketext(T_HERO, "HERO");
	maketext(T_LIVECODED, "Live-coded at Breakpoint 2010");
	maketext(T_MENU, "Press space (or T for trainer)");
	maketext(T_TRAINER, "TRAINER MODE");
	maketext(T_INSTRUCT1, "Avoid dangerous edges.");
	maketext(T_INSTRUCT2, "Press 1 to toggle direction.");
	maketext(T_INSTRUCT3, "ESC to go back. Good luck!");
	maketext(T_1, "1");
	maketext(T_2, "2");
	maketext(T_3, "3");
	maketext(T_4, "4");
}

// *************** Sound *************** 

int16_t synthesize() {
	int i;
	int accum = 0;

	for(i = 0; i < 2; i++) {
		osc[i].phase += osc[i].freq;
		accum += ((osc[i].phase & 0x8000)? -1 : 1) * osc[i].volume;
	}

	return accum << 6;
}

void music() {
	int i;
	static int timer = 0;
	static int octave = 0;
	static int currharm = 0;
	static int lastnote = 0;
	static int harmcount = 32;
	struct harmony {
		int		base;
		int		scale[8];
	} harmony[] = {
		{
			12,
			{24, 24, 26, 28, 29, 31, 36, 36}
		},
		{
			9,
			{21, 24, 26, 28, 31, 33, 35, 36}
		},
		{
			7,
			{23, 24, 26, 26, 28, 31, 35, 38}
		},
		{
			5,
			{21, 24, 26, 28, 29, 33, 36, 36}
		},
		{
			4,
			{23, 26, 28, 28, 31, 35, 35, 36}
		}
	};

	for(i = 0; i < 2; i++) {
		int vol = osc[i].volume;

		vol -= 16;
		if(vol < 0) vol = 0;
		osc[i].volume = vol;
	}

	if(timer) {
		timer--;
	} else {
		if(harmcount) {
			harmcount--;
		} else {
			currharm = rand() % (sizeof(harmony) / sizeof(*harmony));
			harmcount = ((rand() % 3) + 1) * 16;
		}
		osc[0].freq = freqtbl[harmony[currharm].base + (octave? 12 : 0)];
		osc[0].volume = 255;
		if(playing && (rand() % 4)) {
			lastnote += (rand() % 4) - 2;
			lastnote &= 7;
			osc[1].freq = freqtbl[harmony[currharm].scale[lastnote]];
			osc[1].volume = 255;
		}
		octave ^= 1;
		timer = 10;
	}
}

static void load_font() {
	TTF_Init();

#ifdef WIN32
	int len = (int) &binary_Allerta_allerta_medium_ttf_end;
	len -= (int) &binary_Allerta_allerta_medium_ttf_start;
	SDL_RWops *fontdata = SDL_RWFromMem(&binary_Allerta_allerta_medium_ttf_start, len);
#else
	int len = (int) &_binary_Allerta_allerta_medium_ttf_end;
	len -= (int) &_binary_Allerta_allerta_medium_ttf_start;
	SDL_RWops *fontdata = SDL_RWFromMem(&_binary_Allerta_allerta_medium_ttf_start, len);
#endif

	font = TTF_OpenFontRW(fontdata, 1, 100);
}

static int nextpoweroftwo(int x) {
	double logbase2 = log(x) / log(2);
	return round(pow(2,ceil(logbase2)));
}

static void render_text(int tid, double x, double y, double zoom) {
	double w = texturesize[tid][0], h = texturesize[tid][1];

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture[tid]);
	glColor3f(1.0f, 1.0f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE_MINUS_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);

	/* Draw a quad at location */
	glBegin(GL_QUADS);
	glTexCoord2d(0, 0); 
	glVertex2f(x - w/2 * zoom, y);
	glTexCoord2d(1, 0); 
	glVertex2f(x + w/2 * zoom, y);
	glTexCoord2d(1, 1); 
	glVertex2f(x + w/2 * zoom, y + h * zoom);
	glTexCoord2d(0, 1); 
	glVertex2f(x - w/2 * zoom, y + h * zoom);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

// *************** Graphic primitives *************** 

static void fillrect(double x1, double y1, double x2, double y2, double z) {
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glVertex3d(x1, y1, z);
	glVertex3d(x1, y2, z);
	glVertex3d(x2, y2, z);
	glVertex3d(x2, y1, z);
	glEnd();
}

static void draw_circle(double xpos, double ypos, double wspan, double hspan, double z) {
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture[T_CIRCLE]);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_QUADS);
	glTexCoord2d(0, 0);
	glVertex3d(xpos - wspan, ypos - hspan, z);
	glTexCoord2d(0, 1);
	glVertex3d(xpos - wspan, ypos + hspan, z);
	glTexCoord2d(1, 1);
	glVertex3d(xpos + wspan, ypos + hspan, z);
	glTexCoord2d(1, 0);
	glVertex3d(xpos + wspan, ypos - hspan, z);
	glEnd();
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

// *************** Complex drawing operations *************** 

static void draw_doors(struct doors *d) {
	int i;

	// Draw doors on the right side. We're invoked under a flipped transformation in order to draw left doors.

	glColor3d(0, 0, 0);
	fillrect(WALLPOS+0, -ymax, WALLPOS+2, ymax, 50);
	for(i = 0; i < MAXDOOR; i++) {
		if(d->ypos[i] > -ymax) {
			glColor3d(1, 1, 1);
			fillrect(WALLPOS, d->ypos[i] - DOORHEIGHT, OUTERPOS, d->ypos[i], 30);
			fillrect(WALLPOS+4, d->ypos[i] - DOORHEIGHT, OUTERPOS, d->ypos[i], 50);
			glColor3d(0, 0, 0);
			fillrect(WALLPOS, d->ypos[i] - DOORHEIGHT, OUTERPOS, d->ypos[i] - DOORHEIGHT + 2, 50);
			fillrect(WALLPOS, d->ypos[i] - 4, OUTERPOS, d->ypos[i], 50);
			fillrect(WALLPOS+4, d->ypos[i] - DOORHEIGHT, WALLPOS+6, d->ypos[i], 50);
			fillrect(WALLPOS+8, d->ypos[i] - DOORHEIGHT, WALLPOS+10, d->ypos[i], 50);
		}
	}
}

static void draw_shaft(struct shaft *shaft, int tid, struct doors *left, struct doors *right, int xpos) {
	int i, angle, offset;
	double fade = 0;

	if(shaft->animframe > 10) {
		offset = -speed * (shaft->animframe - 10);
		if(shaft->animframe > 60) {
			fade = (shaft->animframe - 60.0) / 20.0;
			if(fade > 1) fade = 1;
		}
	} else {
		offset = shaft->grace;
	}
	glPushMatrix();
		glTranslated(xpos, 0, 0);

		glColor3d(0.83, 0.83, 0.83);
		fillrect(-OUTERPOS, -ymax, OUTERPOS, ymax, 50);
		glColor3d(1, 1, 1);
		fillrect(-WALLPOS-2, -ymax, WALLPOS+2, ymax, 30);

		// Draw the lift
		glPushMatrix();
			glTranslated(0, offset, 0);
			glColor3d(fade, fade, fade);
			fillrect(-WALLPOS, FLOOR, WALLPOS, FLOOR + 2, 30);
			fillrect(-WALLPOS, FLOOR - DOORHEIGHT, WALLPOS, FLOOR - DOORHEIGHT + 2, 30);
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
				// Trash can
				double y = FLOOR - 2;
				y -= shaft->animframe * 0.8;
				if (y < FLOOR-DOORHEIGHT+63) y = FLOOR-DOORHEIGHT + 63;
				glTranslated(28, y, 0);
				angle = shaft->animframe * 2;
				if(angle > 33) angle = 33;
				glTranslated(-angle / 16, -angle / 2, 0);
				glRotated(-angle, 0, 0, 1);
				glColor3d(fade, fade, fade);
				fillrect(-30, -52, 0, 0, 30);
				fillrect(-34, -52, 0, -55, 30);
				glColor3d(1, 1, 1);
				fillrect(-28, -50, -2, -2, 30);
				fillrect(-33, -53, -2, -54, 30);
				glColor3d(fade, fade, fade);
				draw_circle(-29, FLOOR-2, 4, 4, 30);
			glPopMatrix();
			glPushMatrix();
				// Victim head
				y = shaft->animframe * -0.8 + 5;
				if (y < -27) y = -27;
				if (y > 0) y = 0;
				double skew = shaft->animframe * 0.6 - 15;
				if (skew < 0) skew = 0;
				if (skew > 2.6) skew = 2.6;
				glTranslated(0, y, 0);
				glColor3d(fade, fade, fade);
				draw_circle(-19, -62, 9 + skew, 9 - skew, 30);
				// Victim body
				glTranslated(-3 * skew, skew, 0);
				double fall = shaft->animframe  - 60;
				if (fall < 0) fall = 0;
				if (fall > 9) fall = 9;
				glTranslated(skew, 0.3 * fall * fall, 0);
				draw_circle(-19, -32, 9 - skew, 20 + skew, 30);
				fillrect(-16, -15, -23, 0, 30);
			glPopMatrix();

		glPopMatrix();

		// Draw the red lemonade
		glDepthFunc(GL_LESS);
		glColor3d(1, 0, 0);
		if(shaft->direction == RIGHT) glScaled(-1, 1, 1);
		for(i = 0; i < NPARTICLE; i++) {
			if(shaft->particle[i].r > 0 && shaft->particle[i].y < (ymax + CIRCLEMAX) * 8) {
				draw_circle(shaft->particle[i].x / 8, shaft->particle[i].y / 8, 
					shaft->particle[i].r, shaft->particle[i].r, 40 + i*.01);
			}
		}
		glDepthFunc(GL_ALWAYS);

	glPopMatrix();
	render_text(tid, xpos, -ymax + 40, .4);
}

static void drawtitle() {
	glColor3d(0, 0, 0);
	glBegin(GL_TRIANGLES);
	glVertex3d(0, -390, 0);
	glVertex3d(400, 210, 0);
	glVertex3d(-400, 210, 0);
	glEnd();
	glColor3d(.992, 1, .196);
	glBegin(GL_TRIANGLES);
	glVertex3d(0, -360, 0);
	glVertex3d(370, 195, 0);
	glVertex3d(-370, 195, 0);
	glEnd();
	render_text(T_EXCLAM, 0, -320, 1.5);
	render_text(T_KRYO, 0, -180, .5);
	render_text(T_PRESENTS, 0, -120, .4);
	render_text(T_KLAMRISK, 0, -25, .8);
	render_text(T_HERO, 0, 70, 1.3);
	render_text(T_LIVECODED, 0, 240, .4);
	render_text(T_MENU, 0, 290, .4);
	glColor3d(1, 0, 0);
	draw_circle(-78, -35, 6, 6, 0);
	draw_circle(-62, -35, 6, 6, 0);
}

static void drawframe() {
	int i;

	// Set background
	glClearColor(.992, 1, .196, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-640, 640, ymax, -ymax, -100, 100);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_ALWAYS);

	if(!playing) {
		drawtitle();
	} else {
		//todo
		//glColor3d(.47, .47, .47);
		//fillrect(-82, 0, 82, 480);

		for(i = 0; i < nbr_shafts; i++) {
			draw_shaft(&shaft[i], T_1 + i, &doors[i], &doors[i + 1], (160 * (i - 2) + 80) * (nbr_shafts > 1));
		}
		if(nbr_shafts == 1) {
			render_text(T_TRAINER, -600 + texturesize[T_TRAINER][0]/2 * .6, -290, .6);
			render_text(T_INSTRUCT1, -600 + texturesize[T_INSTRUCT1][0]/2 * .3, -200, .3);
			render_text(T_INSTRUCT2, -600 + texturesize[T_INSTRUCT2][0]/2 * .3, -150, .3);
			render_text(T_INSTRUCT3, -600 + texturesize[T_INSTRUCT3][0]/2 * .3, -100, .3);
		}
	}
}

// *************** Gameplay *************** 

static void splat(struct shaft *shaft) {
	int i;

	for(i = 0; i < NPARTICLE; i++) {
		shaft->particle[i].x = SPLATTERPOS * 8;
		shaft->particle[i].y = (FLOOR - 63) * 8;
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
			d->ypos[i] -= speed;
		}
	}
}

static void shaft_physics(struct shaft *shaft, struct doors *left, struct doors *right) {
	int i;

	if(shaft->alive) {
		struct doors *dangerous = (shaft->direction == LEFT)? left : right;

		if(!shaft->grace) {
			for(i = 0; i < MAXDOOR; i++) {
				if(dangerous->ypos[i] > FLOOR - 3 && dangerous->ypos[i] < FLOOR) {
					shaft->alive = 0;
				}
			}
		}
		shaft->grace -= speed;
		if(shaft->grace < 0) shaft->grace = 0;
	} else {
		shaft->animframe++;
		if(shaft->animframe == 15) {
			splat(shaft);
		} else if(shaft->animframe == 45) {
			//speed++;
		} else if(shaft->animframe == 100) {
			shaft->alive = 1;
			shaft->animframe = 0;
			shaft->grace = ymax + DOORHEIGHT;
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
	shaft->grace = 0;
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

static void newgame(int shafts) {
	int i;

	if (shafts > 4)
		return;

	playing = 1;
	nbr_doors = shafts+1;
	nbr_shafts = shafts;
	
	for(i = 0; i < shafts; i++) {
		resetshaft(&shaft[i]);
	}
	for(i = 0; i < shafts + 1; i++) {
		resetdoors(&doors[i]);
	}
	rate = 50;
	appearance_timer = rate;
	speed = 2;
}

static void add_door() {
	int which = rand() % nbr_doors;
	int i;

	for(i = 0; i < MAXDOOR; i++) {
		if(doors[which].ypos[i] <= -ymax) {
			doors[which].ypos[i] = ymax + DOORHEIGHT;
			break;
		}
	}
}

static void add_doors() {
	if(rate) {
		if(appearance_timer) {
			appearance_timer--;
		} else {
			add_door();
			appearance_timer = rate;
		}
	}
}

int main(int argc, char *argv[])
{
	int i;
	int running = 1;
	Uint32 lasttick;

	init_sdl();
	load_font();
	precalc();
	if (!font)
		return 1;
	lasttick = SDL_GetTicks();
	playing = 0;
	menutime = lasttick;
	while (running) {
		SDL_Event event;
		Uint32 now = SDL_GetTicks();
		while(now - lasttick > 20) {
			lasttick += 20;
			for(i = 0; i < nbr_doors; i++) {
				doors_physics(&doors[i]);
			}
			for(i = 0; i < nbr_shafts; i++) {
				shaft_physics(&shaft[i], &doors[i], &doors[i + 1]);
			}
			music();
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
							if (!playing)
								newgame(4);
							break;
						case SDLK_t:
							if (!playing)
								newgame(1);
							break;
						case SDLK_ESCAPE:
							if (playing) {
								playing = 0;
								menutime = lasttick;
							} else {
								running = 0;
							}
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
