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
int octave = 6;

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

			/*
			if (down.c)
				sid.c = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave)],
					127, 
					64
				);
			if (up.c)
				soundKill(sid.c);
			*/

			if (down.c)
				sounds[0].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave)],
					startVolume,
					64
				);
			if (held.c) {
				soundSetVolume(
					sounds[0].sid,
					attackDecaySustain(sounds[0].time)
				);
				sounds[0].time++;
			}
			if (up.c) {
				sounds[0].time = 0;
				soundKill(sounds[0].sid);
			}

			// C# adsr
			if (down.c_sharp)
				sounds[1].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 1],
					startVolume,
					64
				);
			if (held.c_sharp) {
				soundSetVolume(
					sounds[1].sid,
					attackDecaySustain(sounds[1].time)
				);
				sounds[1].time++;
			}
			if (up.c_sharp) {
				sounds[1].time = 0;
				soundKill(sounds[1].sid);
			}

			if (down.d)
				sounds[2].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 2],
					startVolume,
					64
				);
			if (held.d) {
				soundSetVolume(
					sounds[2].sid,
					attackDecaySustain(sounds[2].time)
				);
				sounds[2].time++;
			}
			if (up.d) {
				sounds[2].time = 0;
				soundKill(sounds[2].sid);
			}

			if (down.d_sharp)
				sounds[3].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 3],
					startVolume,
					64
				);
			if (held.d_sharp) {
				soundSetVolume(
					sounds[3].sid,
					attackDecaySustain(sounds[3].time)
				);
				sounds[3].time++;
			}
			if (up.d_sharp) {
				sounds[3].time = 0;
				soundKill(sounds[3].sid);
			}

			if (down.e)
				sounds[4].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 4],
					startVolume,
					64
				);
			if (held.e) {
				soundSetVolume(
					sounds[4].sid,
					attackDecaySustain(sounds[4].time)
				);
				sounds[4].time++;
			}
			if (up.e) {
				sounds[4].time = 0;
				soundKill(sounds[4].sid);
			}

			if (down.f)
				sounds[5].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 5],
					startVolume,
					64
				);
			if (held.f) {
				soundSetVolume(
					sounds[5].sid,
					attackDecaySustain(sounds[5].time)
				);
				sounds[5].time++;
			}
			if (up.f) {
				sounds[5].time = 0;
				soundKill(sounds[5].sid);
			}

			if (down.f_sharp)
				sounds[6].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 6],
					startVolume,
					64
				);
			if (held.f_sharp) {
				soundSetVolume(
					sounds[6].sid,
					attackDecaySustain(sounds[6].time)
				);
				sounds[6].time++;
			}
			if (up.f_sharp) {
				sounds[6].time = 0;
				soundKill(sounds[6].sid);
			}

			if (down.g)
				sounds[7].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 7],
					startVolume,
					64
				);
			if (held.g) {
				soundSetVolume(
					sounds[7].sid,
					attackDecaySustain(sounds[7].time)
				);
				sounds[7].time++;
			}
			if (up.g) {
				sounds[7].time = 0;
				soundKill(sounds[7].sid);
			}

			if (down.g_sharp)
				sounds[8].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 8],
					startVolume,
					64
				);
			if (held.g_sharp) {
				soundSetVolume(
					sounds[8].sid,
					attackDecaySustain(sounds[8].time)
				);
				sounds[8].time++;
			}
			if (up.g_sharp) {
				sounds[8].time = 0;
				soundKill(sounds[8].sid);
			}

			if (down.a)
				sounds[9].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 9],
					startVolume,
					64
				);
			if (held.a) {
				soundSetVolume(
					sounds[9].sid,
					attackDecaySustain(sounds[9].time)
				);
				sounds[9].time++;
			}
			if (up.a) {
				sounds[9].time = 0;
				soundKill(sounds[9].sid);
			}

			if (down.a_sharp)
				sounds[10].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 10],
					startVolume,
					64
				);
			if (held.a_sharp) {
				soundSetVolume(
					sounds[10].sid,
					attackDecaySustain(sounds[10].time)
				);
				sounds[10].time++;
			}
			if (up.a_sharp) {
				sounds[10].time = 0;
				soundKill(sounds[10].sid);
			}

			if (down.b)
				sounds[11].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 11],
					startVolume,
					64
				);
			if (held.b) {
				soundSetVolume(
					sounds[11].sid,
					attackDecaySustain(sounds[11].time)
				);
				sounds[11].time++;
			}
			if (up.b) {
				sounds[11].time = 0;
				soundKill(sounds[11].sid);
			}

			if (down.high_c)
				sounds[12].sid = soundPlayPSG(
					DutyCycle_25,
					pitches[pitch + (12 * octave) + 12],
					startVolume,
					64
				);
			if (held.high_c) {
				soundSetVolume(
					sounds[12].sid,
					attackDecaySustain(sounds[12].time)
				);
				sounds[12].time++;
			}
			if (up.high_c) {
				sounds[12].time = 0;
				soundKill(sounds[12].sid);
			}
		}

	}

	return 0;
}
