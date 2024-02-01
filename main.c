/*---------------------------------------------------------------------------------

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.
	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.
	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/

#include <nds.h>
#include <stdio.h>
#include <nf_lib.h>
#include <nds/arm9/piano.h>

#define SLIDER_MAX_Y 152
#define SLIDER_MIN_Y 24
#define SLIDER_WIDTH 32
#define SLIDER_HEIGHT 64

typedef struct {
	union {
		struct {
			u16 c : 1;
			u16 c_sharp : 1; //rules!
			u16 d : 1;
			u16 d_sharp : 1;
			u16 e : 1;
			u16 f : 1;
			u16 f_sharp : 1;
			u16 g : 1;
			u16 g_sharp : 1;
			u16 a : 1;
			u16 a_sharp : 1;
			u16 _unknown1 : 1;
			u16 _unknown2 : 1;
			u16 b : 1;
			u16 high_c : 1; //is yummy
			u16 _unknown3 : 1;
		};
		u16 VAL;
	};
} PianoKeys;

// used to hold the sound id's of the notes being played
struct SoundID {
	int c;
	int c_sharp;
	int d;
	int d_sharp;
	int e;
	int f;
	int f_sharp;
	int g;
	int g_sharp;
	int a;
	int a_sharp;
	int b;
	int high_c;
};

// soundinfo keeps track of the sound information associated with a single key
struct SoundInfo {
	int sid;
	int time;
};

struct SoundInfo sounds[13];

int maxVolume = 127;
int startVolume = 127;
int sensitivity = 127;

int attackFinish;
int decayFinish;
int releaseFinish;

struct Slider {
	int id;
	int x;
	int y;
	int level;
};

struct Slider attack = { .id = 0, .x = 16, .y = 152, .level = 0 };
struct Slider decay = { .id = 1, .x = 48, .y = 152, .level = 0 };
struct Slider sustain = { .id = 2, .x = 80, .y = 24, .level = 127 };
struct Slider release = { .id = 3, .x = 112, .y = 152, .level = 0 };

int pitches[] = {
	55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104,
	110, 117, 123, 131, 139, 147, 156, 165, 175, 185, 196, 208,
	220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415,
	440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831,
	880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661,
	1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322,
	3520, 3729, 3951, 4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645
};

int pitch = 3;
int octave = 5;

int decideStartVolume() {
	// if decay and attack are both at zero, start volume is sustain.level
	if (!attack.level && !decay.level) {
		maxVolume = sustain.level;
		return sustain.level;
	}
	maxVolume = 127;
	// if attack level is at zero, set the start volume to full
	if (!attack.level) {
		return 127;
	}
	// otherwise, start tone at volume 0
	return 0;
}

void moveSlider(touchPosition *touch, struct Slider *slider) {
	// if the touch is within the x-axis bounds, move it
	if (touch->px >= slider->x && touch->px < slider->x + SLIDER_WIDTH) {
		// make sure the slider stays in its y-axis bounds
		slider->y = touch->py - 32;
		if (slider->y < SLIDER_MIN_Y)
			slider->y = SLIDER_MIN_Y;
		if (slider->y >= SLIDER_MAX_Y)
			slider->y = SLIDER_MAX_Y - 1;

		// do some simple arithmetic to set the level of the slider
		slider->level = 127 - (slider->y - 24);
	}
	NF_MoveSprite(1, slider->id, slider->x, slider->y);

	// set the envelope checkpoints
	attackFinish = (127*attack.level)/sensitivity;
	decayFinish = (127*(decay.level + attack.level) - sustain.level*decay.level)/sensitivity;
	releaseFinish = (sustain.level*release.level)/sensitivity;

	startVolume = decideStartVolume();
};

// oops, algebra came back.
// this does not increment time. do that yourself please
int attackDecaySustain(int time) {
	if (time < attackFinish)
		return (time*sensitivity)/attack.level;
	if (time >= attackFinish && time < decayFinish)
		return -((time*sensitivity)/decay.level) + maxVolume + ((maxVolume*attack.level)/decay.level);
	return sustain.level;
}

// kill all sounds if needed
void killAllSounds() {
	for (int i = 0; i < 13; i++) {
		soundKill(sounds[i].sid);
	}
}

int main(void) {
	consoleDemoInit();

	consoleClear();

	if (isDSiMode()) {
		iprintf("No gamepak addons in DSi mode\n");
	}
	else {
		iprintf("Insert gamepak addon.");
	}

	/*
	struct SoundID sid = {
		.c = -1,
		.c_sharp = -1,
		.d = -1,
		.d_sharp = -1,
		.e = -1,
		.f = -1,
		.f_sharp = -1,
		.g = -1,
		.g_sharp = -1,
		.a = -1,
		.a_sharp = -1,
		.b = -1,
		.high_c = -1
	};
	*/

	touchPosition touch;
	NF_Set2D(1, 0);

	NF_SetRootFolder("NITROFS");
	NF_InitTiledBgBuffers();
	NF_InitTiledBgSys(1);

	NF_LoadTiledBg("ADSR", "ADSR", 256, 256);

	NF_CreateTiledBg(1, 3, "ADSR");

	NF_InitSpriteBuffers();
	NF_InitSpriteSys(1);

	NF_LoadSpriteGfx("MixerSlider", 0, 32, 64);
	NF_LoadSpritePal("psg_a1", 0);

	NF_VramSpriteGfx(1, 0, 0, false);
	NF_VramSpritePal(1, 0, 0);

	NF_CreateSprite(1, 0, 0, 0, 16, 152); // attack
	NF_CreateSprite(1, 1, 0, 0, 48, 152); // decay
	NF_CreateSprite(1, 2, 0, 0, 80, 24); // sustain
	NF_CreateSprite(1, 3, 0, 0, 112, 152); // release

	soundEnable();

	while (1) {
		NF_SpriteOamSet(1);
		swiWaitForVBlank();
		oamUpdate(&oamSub);

		scanKeys();
		touchRead(&touch);
		moveSlider(&touch, &attack);
		moveSlider(&touch, &decay);
		moveSlider(&touch, &sustain);
		moveSlider(&touch, &release);

		int keysD = keysDown();
		if (keysD & KEY_UP)
			octave++;
		if (keysD & KEY_DOWN)
			octave--;
		if (keysD & KEY_RIGHT)
			pitch++;
		if (keysD & KEY_LEFT)
			pitch--;
		if (keysD & KEY_X)
			killAllSounds();

		if (isDSiMode()) continue;


		// for whatever reason, the piano stuff won't work without checking for the guitar grip first
		// oh well.
		guitarGripIsInserted();

		if (pianoIsInserted())
		{
			pianoScanKeys();
			PianoKeys down;
			down.VAL = pianoKeysDown();
			PianoKeys held;
			held.VAL = pianoKeysHeld();
			PianoKeys up;
			up.VAL = pianoKeysUp();

			int bitfieldShift;
			for (int i = 0; i < 13; i++) {
				bitfieldShift = i + ((i >= 11) ? 2 : 0); // because of the gap in the PianoKeys bitfield ¯\_(ツ)_/¯
				if (down.VAL & 1<<bitfieldShift)
					sounds[i].sid = soundPlayPSG(
						DutyCycle_25,
						pitches[pitch + (12 * octave) + i],
						startVolume,
						64
					);
				if (held.VAL & 1<<bitfieldShift) {
					soundSetVolume(
						sounds[i].sid,
						attackDecaySustain(sounds[i].time)
					);
					sounds[i].time++;
				}
				if (up.VAL & 1<<bitfieldShift) {
					sounds[i].time = 0;
					soundKill(sounds[i].sid);
				}
			}

		}

	}

	return 0;
}
