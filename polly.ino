/*
   Polly - a MIDI proxy sketch for Korg Volca 2
   Copyright (C) 2023 Jacob Ulmert

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <Midiboy.h>
#include <EEPROM.h>

#define N_GROUPS 4
#define N_SAMPLES_IN_GROUP 4

uint8_t g_TXavailable = 0;

typedef struct {
  uint8_t midi;
  uint8_t noteStealing;
  uint8_t sampleSelectMode;
  uint8_t sampleNumbers[N_SAMPLES_IN_GROUP];
  uint8_t reverb[N_SAMPLES_IN_GROUP];
  uint8_t pan;
} group_t;

group_t g_groups[N_GROUPS] = {
  { 0, 0, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0},
  { 0, 0, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0},
  { 0, 0, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0},
  { 0, 0, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0}
};

typedef struct {
  uint8_t currentChannelIdx;
  uint8_t sampleCount;
  uint8_t keyDiv;
  uint8_t sampleIdx;
  uint8_t sampleNumbersCompact[N_SAMPLES_IN_GROUP];
  uint8_t panIdx;
} group_state_t;

group_state_t g_groups_state[N_GROUPS];

/*
group_state_t g_groups_state[N_GROUPS] = {
  {0, 0, 0, 0, { 0, 0, 0, 0 }, 0},
  {0, 0, 0, 0, { 0, 0, 0, 0 }, 0},
  {0, 0, 0, 0, { 0, 0, 0, 0 }, 0},
  {0, 0, 0, 0, { 0, 0, 0, 0 }, 0}
};
*/

#define N_CHANNELS 9

#define MODIFIER_DISABLED 0
#define MODIFIER_RND 1
#define MODIFIER_KEY 2
#define MODIFIER_LAST 2

#define MODIFIER_CYCLE 3
#define MODIFIER_LAST_EXT 3

#define PITCHMODE_DISABLED 0
#define PITCHMODE_ON 1
#define PITCHMODE_LAST 1

typedef struct {
  uint8_t midi;
  uint8_t noteStealing;
  uint8_t noteLower;
  uint8_t noteUpper;
  int8_t noteOffset;
  uint8_t release;

  uint8_t velocity;
  uint8_t velocityMin;
  uint8_t velocityMax;
  uint8_t invertVelocity;

  uint8_t hicut;
  uint8_t hicutMin;
  uint8_t hicutMax;

  uint8_t group;

  uint8_t pitchMode;
 
} channel_t;

typedef struct {
  uint8_t noteOn;
  uint8_t midiOn;
} channel_state_t;

channel_state_t g_channels_state[N_CHANNELS];

channel_t g_channels[N_CHANNELS] = {
  { 1, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON},
  { 2, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON},
  { 3, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON},
  { 4, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON},

  { 5, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON},
  { 6, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON},
  { 7, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON},
  { 8, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON},

  { 9, 1, 0, 127, 0, 0, MODIFIER_DISABLED, 1, 127, 0, MODIFIER_DISABLED, 1, 127, 0, PITCHMODE_ON}
};

static const PROGMEM char LABEL_MODIFIER[4][4] = { "OFF", "RND", "KEY", "CYC" };

static const PROGMEM char LABEL_REVERB[4][4] = { "OFF", "ENA", "DIS", "RND" };

static const PROGMEM char LABEL_SAVESLOT[10] = { "SAVE SLOT" };
static const PROGMEM char LABEL_CANCELLED[10] = { "CANCELLED" };
static const PROGMEM char LABEL_SAVED[8] = { "SAVE OK" };
static const PROGMEM char LABEL_LOAD_SUCCESS[8] = { "LOAD OK" };
static const PROGMEM char LABEL_LOAD_FAIL[9] = { "LOAD NOK" };

static const PROGMEM char NOTIF_LOADSAVE[26] = { "HOLD LEFT:SAVE RIGHT:LOAD" };

static const PROGMEM char NOTIF_MIDIONLY[13] = { "LET IT ROLL!" };

static const PROGMEM char NOTIF_COPIEDTOGROUP[16] = { "COPIED TO GROUP" };
static const PROGMEM char NOTIF_NOTCOPIEDTOGROUP[9] = { "NO GROUP" };

static const PROGMEM char LABEL_OFF[4] = { "OFF" };
static const PROGMEM char LABEL_ON[4] = { " ON" };
static const PROGMEM char LABEL_ALL[4] = { "ALL" };
static const PROGMEM char LABEL_CHANNEL[9] = { "CHANNEL " };
static const PROGMEM char LABEL_GROUP[7] = { "GROUP " };
static const PROGMEM char LABEL_COPYTOGROUP[12] = { " A:CPY->GRP" };


static const PROGMEM char LABEL_NOTES[12][3] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };

uint8_t g_renderSection = 0;

inline uint8_t *notePropToString(uint8_t v, uint8_t a, uint8_t *dest) {
  uint8_t i = (v % 12);
  *dest++ = pgm_read_byte(&LABEL_NOTES[i][0]);
  *dest++ = pgm_read_byte(&LABEL_NOTES[i][1]);

  int8_t n = (v / 12 - 1);
  if (n >= 0) {
    *dest++ = ('0' + n);
  } else {
    *dest++ = ('-');
  }
  return dest;
}

uint8_t g_midiMessage[3] = { 0, 0, 0 };
uint8_t g_midiReceived = 0;
bool g_midiDataIgnore = false;

#define GUIPAGE_CHANNEL1 0
#define GUIPAGE_CHANNEL2 1
#define GUIPAGE_CHANNEL3 2
#define GUIPAGE_CHANNEL4 3
#define GUIPAGE_CHANNEL5 4
#define GUIPAGE_CHANNEL6 5
#define GUIPAGE_CHANNEL7 6
#define GUIPAGE_CHANNEL8 7
#define GUIPAGE_CHANNEL9 8

#define GUIPAGE_GROUP1 9
#define GUIPAGE_GROUP2 10
#define GUIPAGE_GROUP3 11
#define GUIPAGE_GROUP4 12

#define GUIPAGE_INIT 13

#define GUIPAGE_PAGELOADSAVE 14
#define GUIPAGE_PAGELAST 15

typedef struct {
  unsigned long timestamp;
  bool inversed;

  uint8_t page;
  int8_t row;

  uint8_t renderOffset;

  uint8_t previewDur;

  uint8_t btnPressedTicks;
  uint8_t btnState;

} gui_t;

gui_t g_gui = {
  0,
  false,
  0,
  0,
  0,
  0,
  0,
};

void setGuiState(uint8_t state);

#define BUTTON_PRESSED 1
#define ACTION_LEFT 2
#define ACTION_RIGHT 4
#define ACTION_A 8
#define BUTTON_LONGPRESS 16

#define SETTING_PROPERTY_REVERB 0b0000010
#define SETTING_PROPERTY_NOTE 0b00000100
#define SETTING_PROPERTY_MIN_OFF 0b00001000
#define SETTING_PROPERTY_SIGNED_VALUE 0b00010000
#define SETTING_PROPERTY_MIN_ALL 0b00100000
#define SETTING_PROPERTY_MAX_ON 0b01000000

#define SETTING_PROPERTY_MODIFIER 0b10000000

typedef struct setting_t {
  const char label[12];
  uint8_t properties;
  int8_t *pVal;
  int8_t minVal;
  int8_t maxVal;
  uint8_t flag;
};

#define N_SETTINGS 15

#define N_SAVESLOTS 4

#define SAVESLOT_SIZE 224

#define FLAG_UPDATE_GROUPS 1
#define FLAG_UPDATE_SAMPLES 2

setting_t g_settings[N_SETTINGS] = {
  { "MIDI   \0", SETTING_PROPERTY_MIN_ALL, (int8_t *)0, 0, 16, 0 },                                       // midi
  { "NSTEAL \0", SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t *)0, 0, 1, 0 },              // Note stealing
  { "LOWER  \0", SETTING_PROPERTY_NOTE, (int8_t *)0, 0, 127, FLAG_UPDATE_GROUPS },                        // noteLower
  { "UPPER  \0", SETTING_PROPERTY_NOTE, (int8_t *)0, 0, 127, FLAG_UPDATE_GROUPS },                        // noteUpper
  { "OFFSET \0", SETTING_PROPERTY_SIGNED_VALUE, (int8_t *)0, -63, 63, FLAG_UPDATE_GROUPS },               // noteOffset
  { "REL    \0", SETTING_PROPERTY_MIN_OFF, (int8_t *)0, 0, 127, 0 },                                      // release
  { "VEL    \0", SETTING_PROPERTY_MODIFIER, (int8_t *)0, 0, MODIFIER_LAST, 0 },                           // velocity
  { "VELMIN \0", 0, (int8_t *)0, 0, 127, 0 },                                                             // velocityMin
  { "VELMAX \0", 0, (int8_t *)0, 0, 127, 0 },                                                             // velocityMax
  { "VELINV \0", SETTING_PROPERTY_MIN_OFF, (int8_t *)0, 0, 1, 0 },                                        // Invert velocity
  { "CUT    \0", SETTING_PROPERTY_MODIFIER, (int8_t *)0, 0, MODIFIER_LAST, 0 },                           // hicut
  { "CUTMIN \0", 0, (int8_t *)0, 0, 127, 0 },                                                             // hicutMin
  { "CUTMAX \0", 0, (int8_t *)0, 0, 127, 0 },                                                             // hicutMax
  { "GROUP  \0", SETTING_PROPERTY_MIN_OFF, (int8_t *)0, 0, N_GROUPS, FLAG_UPDATE_GROUPS },                // group
  { "PITCH  \0", SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t *)0, 0, PITCHMODE_LAST, 0 }  // pitchMode
};

#define N_GROUP_SETTINGS 12
setting_t g_group_settings[N_GROUP_SETTINGS] = {
  { "MIDI   \0", SETTING_PROPERTY_MIN_ALL, (int8_t *)0, 0, 16, 0 },                           // Midi
  { "NSTEAL \0", SETTING_PROPERTY_MIN_OFF | SETTING_PROPERTY_MAX_ON, (int8_t *)0, 0, 1, 0 },  // Note stealing
  { "SMODE  \0", SETTING_PROPERTY_MODIFIER, (int8_t *)0, 0, MODIFIER_LAST_EXT, 0 },           // Sample select mode
  { "SMPL 1 \0", SETTING_PROPERTY_MIN_OFF, (int8_t *)0, 0, 200, FLAG_UPDATE_SAMPLES },        // Sample 1
  { "SMPL 2 \0", SETTING_PROPERTY_MIN_OFF, (int8_t *)0, 0, 200, FLAG_UPDATE_SAMPLES },        // Sample 2
  { "SMPL 3 \0", SETTING_PROPERTY_MIN_OFF, (int8_t *)0, 0, 200, FLAG_UPDATE_SAMPLES },        // Sample 3
  { "SMPL 4 \0", SETTING_PROPERTY_MIN_OFF, (int8_t *)0, 0, 200, FLAG_UPDATE_SAMPLES },        // Sample 4
  { "RVRB 1 \0", SETTING_PROPERTY_REVERB, (int8_t *)0, 0, 3, 0 },                             // Reverb
  { "RVRB 2 \0", SETTING_PROPERTY_REVERB, (int8_t *)0, 0, 3, 0 },                             // Reverb
  { "RVRB 3 \0", SETTING_PROPERTY_REVERB, (int8_t *)0, 0, 3, 0 },                             // Reverb
  { "RVRB 4 \0", SETTING_PROPERTY_REVERB, (int8_t *)0, 0, 3, 0 },                             // Reverb
  { "PAN    \0", SETTING_PROPERTY_MIN_OFF, (int8_t *)0, 0, 4, 0 }                             // Pan
};

void setGroupsSampleNumbers() {
  uint8_t i = N_GROUPS;
  while (i > 0) {
    i--;

    uint8_t j = 0;
    g_groups_state[i].sampleCount = 0;
    while (j < N_SAMPLES_IN_GROUP) {
      if (g_groups[i].sampleNumbers[j]) {
        g_groups_state[i].sampleNumbersCompact[g_groups_state[i].sampleCount] = g_groups[i].sampleNumbers[j] - 1;
        g_groups_state[i].sampleCount++;
      }
      j++;
    }
    if (g_groups_state[i].sampleCount) {
      g_groups_state[i].keyDiv = 127 / g_groups_state[i].sampleCount;
    }
  }
}

void setGroupsChannelIdxs() {
  uint8_t i = N_GROUPS;
  while (i > 0) {
    i--;
    uint8_t j = 0;
    while (j < N_CHANNELS) {
      uint8_t group = g_channels[j].group;
      if (group && (group - 1) == i) {
        g_groups_state[i].currentChannelIdx = j;
        break;
      }
      j++;
    }
  }
}

bool copyChannelToGroups(uint8_t idx) {
  bool copied = false;
  uint8_t group = g_channels[idx].group;
  if (group) {
    uint8_t i = 0;
    while (i < N_CHANNELS) {
      if (i != idx && group == g_channels[i].group) {
        memcpy(&g_channels[i], &g_channels[idx], sizeof(channel_t));
      }
      i++;
    }
    setGroupsSampleNumbers();
    setGroupsChannelIdxs();
    copied = true;
  }
  return copied;
}

void copyChannelToSettings(uint8_t idx) {
  uint8_t i = 0;
  while (i < N_SETTINGS) {
    g_settings[i].pVal = (int8_t *)&g_channels[idx] + i;
    i++;
  }
}

void copySettingsToChannel(uint8_t idx) {
  uint8_t i = 0;
  while (i < N_SETTINGS) {
    *((uint8_t *)(&g_channels[idx] + i)) = (uint8_t) *(g_settings[i].pVal);
    i++;
  }
  memset(&g_channels_state[idx], 0, sizeof(channel_state_t));
}

void copyGroupToSettings(uint8_t idx) {
  uint8_t i = 0;
  while (i < N_GROUP_SETTINGS) {
    g_group_settings[i].pVal = (int8_t *)&g_groups[idx] + i;
    i++;
  }
}

inline uint8_t getScaledValue(float v, uint8_t min, uint8_t max) {
  return (uint8_t)(min + (v / 127) * (float)(max - min));
}

#define FONT MIDIBOY_FONT_5X7

typedef struct printCtx_t {
  uint8_t *pText;
  uint8_t textIdx;
  uint8_t bitmapY;
  uint8_t screenY;
  uint8_t printVal;

  uint8_t printBuf[4];
};
uint8_t g_printBuf[4][14] = { { 'A', 'A', 32, 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 0, 0 }, { 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 0, 0 }, { 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 0, 0 }, { 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'B', 0, 0 } };

printCtx_t g_printCtx[4] = { { &g_printBuf[0][0], 0, 0, 6, 0, { 0, 0, 0, 0 } }, { &g_printBuf[1][0], 0, 0, 4, 0, { 0, 0, 0, 0 } }, { &g_printBuf[2][0], 0, 0, 2, 0, { 0, 0, 0, 0 } }, { &g_printBuf[2][0], 0, 0, 6, 0, { 0, 0, 0, 0 } } };

void printString_P(const char *pText, bool inverted) {
  char v = pgm_read_byte(pText++);
  while (v != 0) {
    Midiboy.drawBitmap_P(&FONT::DATA_P[(v - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
    v = pgm_read_byte(pText++);
  }
}

void printString(const char *pText, bool inverted) {
  while (*pText != 0) {
    Midiboy.drawBitmap_P(&FONT::DATA_P[(*pText++ - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
  }
}

inline uint8_t *bytePropToString(uint8_t v, uint8_t a, uint8_t *dest) {
  uint8_t t = (v / 100);
  if (v < 100) {
    *dest++ = ' ';
  }
  if (v < 10) {
    *dest++ = ' ';
  }

  if (t > 0)
    *dest++ = '0' + t | a;
  v -= (t * 100);
  if (t > 0 || v > 9) {
    t = (v / 10);
    *dest++ = '0' + t | a;
  }
  t = (v - (v / 10 * 10));
  *dest++ = '0' + t | a;
  return dest;
}

inline uint8_t *stringPropToString(const char *text, uint8_t a, uint8_t *dest) {
  uint8_t i = 0;
  uint8_t c = pgm_read_byte(text++);
  while (c != 0) {
    *dest++ = c | a;
    c = pgm_read_byte(text++);
  }
  return dest;
}

void printByte(uint8_t v, bool inverted) {
  uint8_t t = (v / 100);
  if (t > 0)
    Midiboy.drawBitmap_P(&FONT::DATA_P[('0' + t - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
  v -= (t * 100);
  if (t > 0 || v > 9) {
    t = (v / 10);
    Midiboy.drawBitmap_P(&FONT::DATA_P[('0' + t - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
  }
  t = (v - (v / 10 * 10));
  Midiboy.drawBitmap_P(&FONT::DATA_P[('0' + t - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
}

void printByteSigned(int8_t v, bool inverted) {
  if (v > 0) {
    Midiboy.drawBitmap_P(&FONT::DATA_P[('+' - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
  } else if (v < 0) {
    Midiboy.drawBitmap_P(&FONT::DATA_P[('-' - ' ') * FONT::WIDTH], FONT::WIDTH, inverted);
    v = -v;
  }
  printByte(v, inverted);
}

#define INT16_BITS 16

inline uint16_t leftRotate(uint16_t n, uint16_t d) {
  return (n << d) | (n >> (INT16_BITS - d));
}

inline uint16_t rightRotate(uint16_t n, uint16_t d) {
  return (n >> d) | (n << (INT16_BITS - d));
}

enum { EEPROM_SAVE_START_ADDRESS = 20 };

void applySetting(const struct setting_t *pSetting) {
  if (pSetting->flag & FLAG_UPDATE_GROUPS) {
    setGroupsChannelIdxs();
  }
  if (pSetting->flag & FLAG_UPDATE_SAMPLES) {
    setGroupsSampleNumbers();
  }
}

void setup() {

  uint8_t i = 0;
  while (i < N_CHANNELS) {
    memset(&g_groups_state[i], 0, sizeof(channel_state_t));
    memset(&g_channels_state[i],0, sizeof(channel_state_t));

    copyChannelToSettings(i);
    uint8_t j = 0;
    while (j < N_SETTINGS) {
      applySetting((const struct setting_t *)&g_settings[j]);
      j++;
    }
    i++;
  }

  Midiboy.begin();
  Midiboy.setButtonRepeatMs(50);

  randomSeed(millis());

  setGuiState(GUIPAGE_INIT);
}

#define VERSION_SAVE 1
#define LEN_SAVE 0

bool loadSong(uint8_t slotIdx) {

  if (slotIdx >= N_SAVESLOTS) {
    slotIdx = N_SAVESLOTS - 1;
  }

  uint16_t offset = slotIdx * SAVESLOT_SIZE;
  uint16_t d = offset;

  if (EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++)) == VERSION_SAVE) {
    EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++));  // Length

    uint8_t i = 0;
    while (i < N_CHANNELS) {
      uint8_t j = 0;
      while (j < N_SETTINGS) {
        *(g_settings[j].pVal) = EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++));
        applySetting((const struct setting_t *)&g_settings[j]);
        j++;
      }
      copySettingsToChannel(i);
      i++;
    }

    i = 0;
    while (i < N_GROUPS) {
      uint8_t j = 0;
      while (j < N_GROUP_SETTINGS) {
        *((uint8_t*)&g_groups[i] + j) = EEPROM.read(EEPROM_SAVE_START_ADDRESS + (d++));
        j++;
      }
      i++;
    }

    setGroupsSampleNumbers();
    setGroupsChannelIdxs();

    randomSeed(millis());
    return true;
  }
  return false;
}

void saveSong(uint8_t slotIdx) {

  if (slotIdx >= N_SAVESLOTS) {
    slotIdx = N_SAVESLOTS - 1;
  }

  uint16_t offset = slotIdx * SAVESLOT_SIZE;
  uint16_t d = offset;
  EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++), VERSION_SAVE);
  d++; 

  uint8_t i = 0;
  while (i < N_CHANNELS) {
    copyChannelToSettings(i);
    uint8_t j = 0;
    while (j < N_SETTINGS) {
      EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++), *(g_settings[j].pVal));
      j++;
    }
    i++;
  }

  i = 0;
  while (i < N_GROUPS) {
    copyGroupToSettings(i);
    uint8_t j = 0;
    while (j < N_GROUP_SETTINGS) {
      EEPROM.update(EEPROM_SAVE_START_ADDRESS + (d++), *(g_group_settings[j].pVal));
      j++;
    }
    i++;
  }

  EEPROM.update(EEPROM_SAVE_START_ADDRESS + 1, d - offset);
}

void incSetting(const struct setting_t *pSetting) {
  if (pSetting->properties & SETTING_PROPERTY_SIGNED_VALUE) {
    int8_t v = *pSetting->pVal;
    if (v < pSetting->maxVal) {
      v++;
      *pSetting->pVal = v;
    }
  } else {
    uint8_t v = *pSetting->pVal;
    if (v < (uint8_t)pSetting->maxVal) {
      v++;
      *pSetting->pVal = v;
    }
  }
  applySetting(pSetting);
}

void decSetting(const struct setting_t *pSetting) {
  if (pSetting->properties & SETTING_PROPERTY_SIGNED_VALUE) {
    int8_t v = *pSetting->pVal;
    if (v > pSetting->minVal) {
      v--;
      *pSetting->pVal = v;
    }
  } else {
    uint8_t v = *pSetting->pVal;
    if (v > (uint8_t)pSetting->minVal) {
      v--;
      *pSetting->pVal = v;
    }
  }
  applySetting(pSetting);
}

void setSetting(const struct setting_t *pSetting, int8_t v) {
  if (pSetting->properties & SETTING_PROPERTY_SIGNED_VALUE) {
    if (v < pSetting->minVal) {
      v = pSetting->minVal;
    } else if (v > pSetting->maxVal) {
      v = pSetting->maxVal;
    }
  } else {
    if ((uint8_t)v < (uint8_t)pSetting->minVal) {
      v = pSetting->minVal;
    } else if ((uint8_t)v > (uint8_t)pSetting->maxVal) {
      v = pSetting->maxVal;
    }
  }
  *pSetting->pVal = v;
}

void printGroupNumber(uint8_t v) {
  Midiboy.setDrawPosition(0, 0);
  printString_P(&LABEL_GROUP[0], true);
  Midiboy.drawBitmap_P(&FONT::DATA_P[('0' + v - ' ') * FONT::WIDTH], FONT::WIDTH, true);
  
  Midiboy.drawSpace(128 - Midiboy.getDrawPositionX(), false);
  g_gui.previewDur = 8;
}

void printChannelNumber(uint8_t v) {
  Midiboy.setDrawPosition(0, 0);
  printString_P(&LABEL_CHANNEL[0], true);
  Midiboy.drawBitmap_P(&FONT::DATA_P[('0' + v - ' ') * FONT::WIDTH], FONT::WIDTH, true);
  uint8_t x = (128 - Midiboy.getDrawPositionX()) - (FONT::WIDTH * (sizeof(LABEL_COPYTOGROUP) - 1));
  Midiboy.drawSpace(x, false);
  printString_P(&LABEL_COPYTOGROUP[0], false);
  g_gui.previewDur = 8;
}

void printNotification(const char *pText) {
  Midiboy.setDrawPosition(0, 0);
  printString_P(pText, false);
  Midiboy.drawSpace(128 - Midiboy.getDrawPositionX(), false);
  g_gui.previewDur = 8;
}

void setGuiState(uint8_t state) {
  if (state >= GUIPAGE_PAGELAST) {
    state = 0;
  }
  if (g_gui.page != state) {
    g_gui.page = state;

    Midiboy.clearScreen();
    if (g_gui.page == GUIPAGE_INIT) {

    } else if (g_gui.page == GUIPAGE_PAGELOADSAVE) {
      printNotification(&NOTIF_LOADSAVE[0]);
    } else if (g_gui.page >= GUIPAGE_GROUP1) {
      copyGroupToSettings(state - GUIPAGE_GROUP1);
      printGroupNumber(state - GUIPAGE_GROUP1 + 1);
    } else if (g_gui.page >= GUIPAGE_CHANNEL1) {
      copyChannelToSettings(state - GUIPAGE_CHANNEL1);
      printChannelNumber(state - GUIPAGE_CHANNEL1 + 1);
    }

    uint8_t i = 0;
    while (i < 3) {
      g_printCtx[i].textIdx = 0;
      g_printCtx[i].printVal = false;
      i++;
    }

    setGuiRow(g_gui.row);
  }
}

void setSelectedItem(int8_t v) {
  if (g_gui.page == GUIPAGE_INIT) {

  } else if (g_gui.page == GUIPAGE_PAGELOADSAVE) {

  } else if (g_gui.page >= GUIPAGE_GROUP1) {
    setSetting(&g_group_settings[g_gui.row], v);
  } else if (g_gui.page >= GUIPAGE_CHANNEL1) {
    setSetting(&g_settings[g_gui.row], v);
  }
}

void decSelectedItem() {
  if (g_gui.page == GUIPAGE_INIT) {

  } else if (g_gui.page == GUIPAGE_PAGELOADSAVE) {

  } else if (g_gui.page >= GUIPAGE_GROUP1) {
    decSetting(&g_group_settings[g_gui.row]);
  } else if (g_gui.page >= GUIPAGE_CHANNEL1) {
    decSetting(&g_settings[g_gui.row]);
  }
}

void incSelectedItem() {
  if (g_gui.page == GUIPAGE_INIT) {

  } else if (g_gui.page == GUIPAGE_PAGELOADSAVE) {

  } else if (g_gui.page >= GUIPAGE_GROUP1) {
    incSetting(&g_group_settings[g_gui.row]);
  } else if (g_gui.page >= GUIPAGE_CHANNEL1) {
    incSetting(&g_settings[g_gui.row]);
  }
}


void setGuiRow(int8_t row) {
  if (row <= 0) {
    g_gui.row = 0;
  } else {
    if (g_gui.page == GUIPAGE_INIT) {
      row = 0;

    } else if (g_gui.page == GUIPAGE_PAGELOADSAVE) {
      if (row > (N_SAVESLOTS - 1)) {
        row = (N_SAVESLOTS - 1);
      }
    } else if (g_gui.page >= GUIPAGE_GROUP1) {
      if (row > (N_GROUP_SETTINGS - 1)) {
        row = (N_GROUP_SETTINGS - 1);
      }
    } else if (g_gui.page >= GUIPAGE_CHANNEL1) {
      if (row > (N_SETTINGS - 1)) {
        row = (N_SETTINGS - 1);
      }
    }
    g_gui.row = row;
  }
}

void handleInput() {
  MidiboyInput::Event event;
  while (Midiboy.readInputEvent(event)) {

    if (event.m_type == MidiboyInput::EVENT_DOWN) {

      g_gui.btnState |= BUTTON_PRESSED;

      switch (event.m_button) {
        case MidiboyInput::BUTTON_RIGHT:
          if (g_gui.page == GUIPAGE_PAGELOADSAVE) {
            if (!(g_gui.btnState & ACTION_RIGHT)) {
              g_gui.btnState |= BUTTON_LONGPRESS;
            }
          } else {
            incSelectedItem();
          }
          g_gui.btnState |= ACTION_RIGHT;
          break;

        case MidiboyInput::BUTTON_LEFT:
          if (g_gui.page == GUIPAGE_PAGELOADSAVE) {
            if (!(g_gui.btnState & ACTION_LEFT)) {
              g_gui.btnState |= BUTTON_LONGPRESS;
            }
          } else {
            decSelectedItem();
          }
          g_gui.btnState |= ACTION_LEFT;
          break;

        case MidiboyInput::BUTTON_DOWN:
          setGuiRow(g_gui.row + 1);
          break;

        case MidiboyInput::BUTTON_UP:
          setGuiRow(g_gui.row - 1);
          break;

        case MidiboyInput::BUTTON_A:
          if (g_gui.page >= GUIPAGE_CHANNEL1 && g_gui.page <= GUIPAGE_CHANNEL8) {
            if (copyChannelToGroups(g_gui.page - GUIPAGE_CHANNEL1)) {
              printNotification(&NOTIF_COPIEDTOGROUP[0]);
            } else {
              printNotification(&NOTIF_NOTCOPIEDTOGROUP[0]);
            }
          } else if (g_gui.page == GUIPAGE_INIT) {
              uint8_t i = N_CHANNELS;
              while (i > 0) {
                i--;
                g_channels_state[i].noteOn = 0;
              }
          }
          break;

        case MidiboyInput::BUTTON_B:
          setGuiState(g_gui.page + 1);
          break;
      }
    } else if (event.m_type == MidiboyInput::EVENT_UP) {

      if (g_gui.page == GUIPAGE_PAGELOADSAVE && g_gui.btnState & BUTTON_LONGPRESS) {
        printNotification(&NOTIF_LOADSAVE[0]);
        g_gui.previewDur = 0;
      }

      g_gui.btnState &= (0xff ^ (BUTTON_PRESSED | BUTTON_LONGPRESS));

      switch (event.m_button) {
        case MidiboyInput::BUTTON_LEFT:
          g_gui.btnState &= (0xff ^ ACTION_LEFT);
          break;

        case MidiboyInput::BUTTON_RIGHT:
          g_gui.btnState &= (0xff ^ ACTION_RIGHT);
          break;
      }
    }
  }
}

void printSaveslot(struct printCtx_t *pCtx, uint8_t slotIdx, bool inverted) {

  char c;
  if (!pCtx->printVal) {
    c = pgm_read_byte(LABEL_SAVESLOT + pCtx->textIdx);
  } else {
    c = pCtx->printBuf[pCtx->textIdx];
  }

  uint8_t *pFontBitmap = (uint8_t *)&FONT::DATA_P[(c - ' ') * FONT::WIDTH];

  Midiboy.setDrawPosition((pCtx->textIdx + pCtx->printVal * 9.8) * (FONT::WIDTH * 2), (pCtx->bitmapY / 4) + pCtx->screenY - 1);

  uint16_t bm = 0;

  uint8_t x = 0;
  while (x != FONT::WIDTH) {
    uint8_t b = pgm_read_byte(pFontBitmap + x);
    if (b & (1 << pCtx->bitmapY - 4)) {
      bm = 0b0000001100000011;
    } else {
      bm = 0;
    }
    if (b & (1 << (pCtx->bitmapY - 3))) {
      bm |= 0b0000110000001100;
    }
    if (b & (1 << (pCtx->bitmapY - 2))) {
      bm |= 0b0011000000110000;
    }
    if (b & (1 << (pCtx->bitmapY - 1))) {
      bm |= 0b1100000011000000;
    }
    Midiboy.drawBitmap((uint8_t *)&bm, 2, inverted);
    x++;
  }
  if (pCtx->bitmapY <= 4) {
    pCtx->bitmapY = 8;
    pCtx->textIdx++;
    if (!pCtx->printVal) {
      if (pgm_read_byte(LABEL_SAVESLOT + pCtx->textIdx) == 0) {
        *bytePropToString(slotIdx, 0, &pCtx->printBuf[0]) = 0;
        pCtx->printVal = true;
        pCtx->textIdx = 0;
      }
    } else if (pCtx->printBuf[pCtx->textIdx] == 0) {
      pCtx->textIdx = 0;
      pCtx->printVal = false;
    }
  } else {
    pCtx->bitmapY -= 4;
  }
}

void printSetting(uint8_t settingIdx, struct printCtx_t *pCtx, const struct setting_t *pSetting, bool inverted) {

  char c;
  if (!pCtx->printVal) {
    c = pSetting->label[pCtx->textIdx];
  } else {
    c = pCtx->printBuf[pCtx->textIdx];
  }

  uint8_t *pFontBitmap = (uint8_t *)&FONT::DATA_P[(c - ' ') * FONT::WIDTH];

  Midiboy.setDrawPosition((pCtx->textIdx + pCtx->printVal * 9) * (FONT::WIDTH * 2), (pCtx->bitmapY / 4) + pCtx->screenY - 1);

  uint16_t bm = 0;

  uint8_t x = 0;
  while (x != FONT::WIDTH) {
    uint8_t b = pgm_read_byte(pFontBitmap + x);
    if (b & (1 << pCtx->bitmapY - 4)) {
      bm = 0b0000001100000011;
    } else {
      bm = 0;
    }
    if (b & (1 << (pCtx->bitmapY - 3))) {
      bm |= 0b0000110000001100;
    }
    if (b & (1 << (pCtx->bitmapY - 2))) {
      bm |= 0b0011000000110000;
    }
    if (b & (1 << (pCtx->bitmapY - 1))) {
      bm |= 0b1100000011000000;
    }
    Midiboy.drawBitmap((uint8_t *)&bm, 2, inverted);
    x++;
  }
  if (pCtx->bitmapY <= 4) {
    pCtx->bitmapY = 8;
    pCtx->textIdx++;
    if (!pCtx->printVal) {
      if (pSetting->label[pCtx->textIdx] == 0) {
        if (pSetting->pVal) {
          if (pSetting->properties & SETTING_PROPERTY_MAX_ON && *pSetting->pVal == pSetting->maxVal) {
            *stringPropToString(&LABEL_ON[0], 0, &pCtx->printBuf[0]) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_REVERB) {
            *stringPropToString(&LABEL_REVERB[*pSetting->pVal][0], 0, &pCtx->printBuf[0]) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_MODIFIER) {
            *stringPropToString(&LABEL_MODIFIER[*pSetting->pVal][0], 0, &pCtx->printBuf[0]) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_MIN_OFF && *pSetting->pVal == pSetting->minVal) {
            *stringPropToString(&LABEL_OFF[0], 0, &pCtx->printBuf[0]) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_MIN_ALL && *pSetting->pVal == pSetting->minVal) {
            *stringPropToString(&LABEL_ALL[0], 0, &pCtx->printBuf[0]) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_NOTE) {
            *(notePropToString(*pSetting->pVal, 0, &pCtx->printBuf[0])) = 0;
          } else if (pSetting->properties & SETTING_PROPERTY_SIGNED_VALUE) {
            if (*pSetting->pVal < 0) {
              *bytePropToString(-(*pSetting->pVal), 0, &pCtx->printBuf[0]) = 0;
              pCtx->printBuf[0] = '-';
            } else {
              *bytePropToString(*pSetting->pVal, 0, &pCtx->printBuf[0]) = 0;
            }
          } else {
            *bytePropToString(*pSetting->pVal, 0, &pCtx->printBuf[0]) = 0;
          }
          pCtx->printVal = true;
        }
        pCtx->textIdx = 0;
      }
    } else if (pCtx->printBuf[pCtx->textIdx] == 0) {
      pCtx->textIdx = 0;
      pCtx->printVal = false;
    }
  } else {
    pCtx->bitmapY -= 4;
  }
}

void pollMidi() {
  while (Midiboy.dinMidi().available()) {
    uint8_t b = Midiboy.dinMidi().read();
    if (b & 0x80) {
      if ((b & 0xf0) == 0x90 || (b & 0xf0) == 0x80) {
        g_midiReceived = 0;
        g_midiDataIgnore = false;
      } else {
        g_midiDataIgnore = true;
      }
      /*
        // 0xfa, 0xfc
        if (b == 0xf8) {
          Midiboy.dinMidi().write(b);  
        }
      */
    }

    if (g_midiDataIgnore == false) {
      if (g_midiReceived >= 3) {
        g_midiReceived = 1;
      }

      g_midiMessage[g_midiReceived++] = b;
      if (g_midiReceived >= 3) {
        if ((g_midiMessage[0] & 0xf0) == 0x90) {

          uint8_t midiChannel = (g_midiMessage[0] & 0x0f) + 1;

          uint8_t midiNoteOn = g_midiMessage[1];
        
          uint8_t i = 0;
          while (i < N_GROUPS) {
            uint8_t j = g_groups_state[i].currentChannelIdx + 1;
            if (j >= N_CHANNELS) {
              j = 0;
            }
            uint8_t k = N_CHANNELS;
            while (k > 0) {
              k--;
              uint8_t group = g_channels[j].group;
              if (group == (i + 1)) {
                if ((!group && (!g_channels[j].midi || g_channels[j].midi == midiChannel)) || (group && (!g_groups[group - 1].midi || g_groups[group - 1].midi == midiChannel))) {
                  if (midiNoteOn >= g_channels[j].noteLower && midiNoteOn <= g_channels[j].noteUpper) {
                    int16_t note = midiNoteOn + g_channels[j].noteOffset;
                    if (note >= 0 && note < 128) {
                      if (g_groups[i].noteStealing || g_channels_state[j].noteOn == 0) {
                        g_groups_state[i].currentChannelIdx = j;
                        break;
                      }
                    }
                  }
                }
              }
              j++;
              if (j >= N_CHANNELS) {
                j = 0;
              }
            }
            i++;
          }

          i = 0;
          while (i < N_CHANNELS) {
            uint8_t midiNoteVelocity = g_midiMessage[2];

            uint8_t group = g_channels[i].group;
            if ((!group && (!g_channels[i].midi || g_channels[i].midi == midiChannel)) || (group && (!g_groups[group - 1].midi || g_groups[group - 1].midi == midiChannel))) {

              if (midiNoteOn >= g_channels[i].noteLower && midiNoteOn <= g_channels[i].noteUpper) {
                int16_t note = midiNoteOn + g_channels[i].noteOffset;
                if (note >= 0 && note < 128) {

                  bool writeNote = false;

                  if (group) {
                    if (g_groups_state[group - 1].currentChannelIdx == i) {
                      if (g_groups[group - 1].noteStealing || !g_channels_state[i].noteOn) {
                        writeNote = true;
                      }
                    }
                  } else {
                    if (g_channels[i].noteStealing || !g_channels_state[i].noteOn) {
                      writeNote = true;
                    }
                  }

                  if (writeNote) {
                    bool controlWritten = false;
                    if (group) {
                      group--;
                      if (g_groups[group].sampleSelectMode != MODIFIER_DISABLED) {
                        if (g_groups_state[group].sampleCount) {
                          uint8_t s;
                          uint8_t sampleIdx;
                          if (g_groups[group].sampleSelectMode == MODIFIER_KEY) {
                            uint8_t keyDiv = g_groups_state[group].keyDiv;
                            s = midiNoteVelocity / keyDiv;
                            if (s >= g_groups_state[group].sampleCount) {
                              s--;
                            }
                            sampleIdx = s;
                            s = g_groups_state[group].sampleNumbersCompact[sampleIdx];
                            midiNoteVelocity = (unsigned char) 127.f * ((float)(midiNoteVelocity % keyDiv) / (float)g_groups_state[group].keyDiv);

                          } else if (g_groups[group].sampleSelectMode == MODIFIER_CYCLE) {
                            sampleIdx = g_groups_state[group].sampleIdx;
                            s = g_groups_state[group].sampleNumbersCompact[sampleIdx];
                            g_groups_state[group].sampleIdx++;
                            if (g_groups_state[group].sampleIdx >= g_groups_state[group].sampleCount) {
                              g_groups_state[group].sampleIdx = 0;
                            }
                          } else {  // MODIFIER_RND
                            g_groups_state[group].sampleIdx = random(0, g_groups_state[group].sampleCount);
                            s = g_groups_state[group].sampleNumbersCompact[g_groups_state[group].sampleIdx];
                            sampleIdx = g_groups_state[group].sampleIdx;
                          }

                          Midiboy.dinMidi().write(0xB0 | i);
                          controlWritten = true;

                          Midiboy.dinMidi().write(3);
                          if (s > 100) {
                            Midiboy.dinMidi().write(1);
                          } else {
                            Midiboy.dinMidi().write((uint8_t)0);
                          }

                          Midiboy.dinMidi().write(35);
                          Midiboy.dinMidi().write(s % 100);

                          if (g_groups[group].reverb[sampleIdx]) {
                            Midiboy.dinMidi().write(70);
                            if (g_groups[group].reverb[sampleIdx] == 1) {
                              Midiboy.dinMidi().write(127);
                            } else if (g_groups[group].reverb[sampleIdx] == 2) {
                              Midiboy.dinMidi().write((uint8_t)0);
                            } else { // RND
                              Midiboy.dinMidi().write(random(0, 2) * 127);
                            }
                          }
                        }
                      }
                      if (g_groups[group].pan) {
                        if (!controlWritten) {
                          Midiboy.dinMidi().write(0xB0 | i);
                          controlWritten = true;
                        }
                        Midiboy.dinMidi().write(10);
                        uint8_t p = 127 >> g_groups[group].pan;
                        if (g_groups_state[group].panIdx) {
                          Midiboy.dinMidi().write((uint8_t)63 - p);
                        } else {
                          Midiboy.dinMidi().write(64 + p);
                        }
                        g_groups_state[group].panIdx ^= 1;
                      }
                    }

                    if (g_channels[i].pitchMode) {
                      if (!controlWritten) {
                        Midiboy.dinMidi().write(0xB0 | i);
                        controlWritten = true;
                      }
                      Midiboy.dinMidi().write(49);
                      Midiboy.dinMidi().write((uint8_t)note);
                    }

                    if (g_channels[i].release) {
                      if (!controlWritten) {
                        Midiboy.dinMidi().write(0xB0 | i);
                        controlWritten = true;
                      }
                      Midiboy.dinMidi().write(48);
                      Midiboy.dinMidi().write(127);
                    }
                    
                    if (g_channels[i].velocity != MODIFIER_DISABLED) {
                      if (!controlWritten) {Midiboy.dinMidi().write(0xB0 | i); controlWritten = true;}
                      Midiboy.dinMidi().write(7);
                      if (g_channels[i].velocity == MODIFIER_KEY) {
                        if (g_channels[i].invertVelocity) {
                          Midiboy.dinMidi().write(getScaledValue((float)(127 - midiNoteVelocity), g_channels[i].velocityMin, g_channels[i].velocityMax));
                        } else {
                          Midiboy.dinMidi().write(getScaledValue((float)midiNoteVelocity, g_channels[i].velocityMin, g_channels[i].velocityMax));
                        }
                      } else if (g_channels[i].velocity == MODIFIER_RND) {
                        Midiboy.dinMidi().write(random(g_channels[i].velocityMin, g_channels[i].velocityMax + 1));
                      }
                    }

                    if (g_channels[i].hicut != MODIFIER_DISABLED) {
                      if (!controlWritten) {
                        Midiboy.dinMidi().write(0xB0 | i);
                        controlWritten = true;
                      }
                      Midiboy.dinMidi().write(42);
                      if (g_channels[i].hicut == MODIFIER_KEY) {
                        Midiboy.dinMidi().write(getScaledValue((float)midiNoteVelocity, g_channels[i].hicutMin, g_channels[i].hicutMax));
                      } else if (g_channels[i].hicut == MODIFIER_RND) {
                        Midiboy.dinMidi().write(random(g_channels[i].hicutMin, g_channels[i].hicutMax + 1));
                      }
                    }

                    Midiboy.dinMidi().write(0x90 | i);
                    Midiboy.dinMidi().write(48 + i);
                    Midiboy.dinMidi().write(127);

                    g_channels_state[i].noteOn = midiNoteOn;
                    g_channels_state[i].midiOn = midiChannel;
                  }
                }
              }
            }
            i++;
          }
        } else if ((g_midiMessage[0] & 0xf0) == 0x80) {
          uint8_t midiChannel = (g_midiMessage[0] & 0x0f) + 1;

          uint8_t noteOff = g_midiMessage[1];

          uint8_t i = N_CHANNELS;
          while (i > 0) {
            i--;
            if (g_channels_state[i].noteOn == noteOff && g_channels_state[i].midiOn == midiChannel) {
              if (g_channels[i].release) {
                Midiboy.dinMidi().write(0xB0 | i);
                Midiboy.dinMidi().write(48);
                Midiboy.dinMidi().write(g_channels[i].release);
              }
              g_channels_state[i].noteOn = 0;
            }
          }
        }
      }
    }
  }
}

void renderSaveSlots() {
  uint8_t slotOffset = 0;
  if (g_gui.row > 2) {
    slotOffset = (g_gui.row - 2);
  }
  uint8_t i = 0;
  while (i < 3) {
    if (g_renderSection == i) {
      printSaveslot(&g_printCtx[i], slotOffset + i, slotOffset + i == g_gui.row);
    }
    i++;
  }
}

void renderSettings(setting_t *pSettings) {
  uint8_t settingsOfst = 0;
  if (g_gui.row > 2) {
    settingsOfst = (g_gui.row - 2);
  }
  uint8_t i = 0;
  while (i < 3) {
    if (g_renderSection == i) {
      printSetting(settingsOfst + i, &g_printCtx[i], &pSettings[settingsOfst + i], settingsOfst + i == g_gui.row);
    }
    i++;
  }
}

void renderPress() {
  Midiboy.setDrawPosition(0, 0);
  uint8_t i = 0;
  while (i < 16) {
    Midiboy.drawSpace(8, i < g_gui.btnPressedTicks);
    i++;
  }
}

void loop() {
  unsigned long timestamp = millis();

  if (timestamp - g_gui.timestamp >= (30)) {
    if (g_TXavailable < 128) {
      g_TXavailable++;
    }
    if (g_gui.btnState & BUTTON_PRESSED) {
      if (g_gui.btnPressedTicks < 16) {
        g_gui.btnPressedTicks++;
        if (g_gui.btnPressedTicks == 16) {
          if (g_gui.btnState & BUTTON_LONGPRESS) {
            if (g_gui.page == GUIPAGE_PAGELOADSAVE) {
              if (g_gui.btnState & ACTION_LEFT) {
                saveSong(g_gui.row);
                printNotification(&LABEL_SAVED[0]);
              } else if (g_gui.btnState & ACTION_RIGHT) {
                if (loadSong(g_gui.row)) {
                  printNotification(&LABEL_LOAD_SUCCESS[0]);
                } else {
                  printNotification(&LABEL_LOAD_FAIL[0]);
                }
              }
            }
            g_gui.btnState &= (0xff ^ BUTTON_LONGPRESS);
          }
        }
      }
    } else {
      g_gui.btnPressedTicks = 0;
    }

    g_gui.inversed ^= true;
    g_gui.timestamp = timestamp;

    if (g_gui.previewDur) {
      g_gui.previewDur--;
    }
  }

  Midiboy.think();

  pollMidi();

  handleInput();

  if (!g_gui.previewDur && g_gui.btnState & BUTTON_LONGPRESS) {
    renderPress();
  } else {
    if (g_gui.page == GUIPAGE_PAGELOADSAVE) {
      if (g_renderSection != 3) {
        renderSaveSlots();
      }
    } else if (g_gui.page == GUIPAGE_INIT) {

      uint8_t v = Midiboy.dinMidi().availableForWrite();
      if (v < g_TXavailable) {
        g_TXavailable = v;
      }

      Midiboy.setDrawPosition(0, 0);
      if (g_TXavailable > 0) {
        Midiboy.drawSpace(g_TXavailable + g_TXavailable, true);
      }
      Midiboy.drawSpace(128 - Midiboy.getDrawPositionX(), false);

    } else if (g_gui.page >= GUIPAGE_GROUP1) {
      renderSettings(&g_group_settings[0]);
    } else if (g_gui.page >= GUIPAGE_CHANNEL1) {
      renderSettings(&g_settings[0]);
    }
  }

  g_renderSection++;
  if (g_renderSection > 4) {
    g_renderSection = 0;
  }
}
