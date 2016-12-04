// lots of copypasta from https://github.com/mit-mit-randomprojectlab/keyboard_multitone
// which in turn uses code from Teensy's IntervalTimer.cpp

static uint32_t tone_toggle_count0;
static uint32_t tone_toggle_count1;
static uint32_t tone_toggle_count2;
static uint32_t tone_toggle_count3;

static volatile uint8_t *tone_reg0;
static volatile uint8_t *tone_reg1;
static volatile uint8_t *tone_reg2;
static volatile uint8_t *tone_reg3;

static uint8_t tone_state0=0;
static uint8_t tone_state1=0;
static uint8_t tone_state2=0;
static uint8_t tone_state3=0;

static float tone_usec0=0.0;
static float tone_usec1=0.0;
static float tone_usec2=0.0;
static float tone_usec3=0.0;

#define TONE_CLEAR_PIN0   tone_reg0[0] = 1
#define TONE_CLEAR_PIN1   tone_reg1[0] = 1
#define TONE_CLEAR_PIN2   tone_reg2[0] = 1
#define TONE_CLEAR_PIN3   tone_reg3[0] = 1

#define TONE_TOGGLE_PIN0  tone_reg0[128] = 1
#define TONE_TOGGLE_PIN1  tone_reg1[128] = 1
#define TONE_TOGGLE_PIN2  tone_reg2[128] = 1
#define TONE_TOGGLE_PIN3  tone_reg3[128] = 1

#define TONE_OUTPUT_PIN0  tone_reg0[384] = 1
#define TONE_OUTPUT_PIN1  tone_reg1[384] = 1
#define TONE_OUTPUT_PIN2  tone_reg2[384] = 1
#define TONE_OUTPUT_PIN3  tone_reg3[384] = 1

uint8_t soundpins[] = { 8, 17, 18, 19 };

static bool PIT_enabled = false;

int currentNote = 0;
int currentChannel = 0;

void enable_PIT() {
  SIM_SCGC6 |= SIM_SCGC6_PIT;
  PIT_MCR = 0;
  PIT_enabled = true;
}

typedef volatile uint32_t* reg;
reg PIT_LDVAL;
reg PIT_TCTRL;
uint8_t IRQ_PIT_CH;

void start_PIT(uint8_t PIT_id, uint32_t newValue) {
  // point to the correct registers
  PIT_LDVAL = &PIT_LDVAL0 + PIT_id * 4;
  PIT_TCTRL = &PIT_TCTRL0 + PIT_id * 4;

  // write value to register and enable interrupt
  *PIT_TCTRL = 0;
  *PIT_LDVAL = newValue;
  *PIT_TCTRL = 3;

  IRQ_PIT_CH = IRQ_PIT_CH0 + PIT_id;
  NVIC_SET_PRIORITY(IRQ_PIT_CH, 128);
  NVIC_ENABLE_IRQ(IRQ_PIT_CH);
}

// Interupt functions
void pit0_isr() { 
  PIT_TFLG0 = 1; 
  TONE_TOGGLE_PIN0;
  tone_toggle_count0--;
  if (tone_toggle_count0 == 0xFFFFFFFB) tone_toggle_count0 = 0xFFFFFFFD;
}
void pit1_isr() { 
  PIT_TFLG1 = 1; 
  TONE_TOGGLE_PIN1;
  tone_toggle_count1--;
  if (tone_toggle_count1 == 0xFFFFFFFB) tone_toggle_count1 = 0xFFFFFFFD;
}
void pit2_isr() { 
  PIT_TFLG2 = 1; 
  TONE_TOGGLE_PIN2;
  tone_toggle_count2--;
  if (tone_toggle_count2 == 0xFFFFFFFB) tone_toggle_count2 = 0xFFFFFFFD;
}
void pit3_isr() { 
  PIT_TFLG3 = 1; 
  TONE_TOGGLE_PIN3;
  tone_toggle_count3--;
  if (tone_toggle_count3 == 0xFFFFFFFB) tone_toggle_count3 = 0xFFFFFFFD;
}

void tone_multi(uint8_t channel, uint16_t frequency, uint32_t duration) {
  uint32_t count;
  volatile uint32_t *config;
  float usec;
  uint32_t newValue;

  uint8_t pin = soundpins[channel];

  if (pin >= CORE_NUM_DIGITAL) return;
  if (duration > 0) {
    count = (frequency * duration / 1000) * 2;
    if (!(count & 1)) count++; // always full waveform cycles
  } else {
    count = 0xFFFFFFFD;
  }
  usec = (float)500000.0 / (float)frequency;
  config = portConfigRegister(pin);

  __disable_irq();
  if (channel == 0) {
    tone_state0 = 1;
    tone_reg0 = portClearRegister(pin);
    TONE_CLEAR_PIN0; // clear pin
    TONE_OUTPUT_PIN0; // output mode;
    *config = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
    tone_toggle_count0 = count;
    tone_usec0 = usec;
    newValue = (float)(F_BUS / 1000000) * usec - 0.5;
    if (!PIT_enabled) enable_PIT();
    start_PIT(channel,newValue);
  } else if (channel == 1) {
    tone_state1 = 1;
    tone_reg1 = portClearRegister(pin);
    TONE_CLEAR_PIN1; // clear pin
    TONE_OUTPUT_PIN1; // output mode;
    *config = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
    tone_toggle_count1 = count;
    tone_usec1 = usec;
    newValue = (float)(F_BUS / 1000000) * usec - 0.5;
    if (!PIT_enabled) enable_PIT();
    start_PIT(channel,newValue);
  } else if (channel == 2) {
    tone_state2 = 1;
    tone_reg2 = portClearRegister(pin);
    TONE_CLEAR_PIN2; // clear pin
    TONE_OUTPUT_PIN2; // output mode;
    *config = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
    tone_toggle_count2 = count;
    tone_usec2 = usec;
    newValue = (float)(F_BUS / 1000000) * usec - 0.5;
    if (!PIT_enabled) enable_PIT();
    start_PIT(channel,newValue);
  } else {
    tone_state3 = 1;
    tone_reg3 = portClearRegister(pin);
    TONE_CLEAR_PIN3; // clear pin
    TONE_OUTPUT_PIN3; // output mode;
    *config = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
    tone_toggle_count3 = count;
    tone_usec3 = usec;
    newValue = (float)(F_BUS / 1000000) * usec - 0.5;
    if (!PIT_enabled) enable_PIT();
    start_PIT(channel,newValue);
  }
  __enable_irq();
}

void noTone_multi(uint8_t channel) {
  uint8_t pin = soundpins[channel];
  if (pin >= CORE_NUM_DIGITAL) return;
  __disable_irq();

  PIT_TCTRL = &PIT_TCTRL0 + channel * 4;
  IRQ_PIT_CH = IRQ_PIT_CH0 + channel;
  *PIT_TCTRL = 0;
  NVIC_DISABLE_IRQ(IRQ_PIT_CH);

  if (channel == 0) {
    if (tone_state0 == 1) {
      TONE_CLEAR_PIN0;
      tone_state0 = 0;
    }
  } else if (channel == 1) {
    if (tone_state1 == 1) {
      TONE_CLEAR_PIN1;
      tone_state1 = 0;
    }
  } else if (channel == 2) {
    if (tone_state2 == 1) {
      TONE_CLEAR_PIN2;
      tone_state2 = 0;
    }
  } else {
    if (tone_state3 == 1) {
      TONE_CLEAR_PIN3;
      tone_state3 = 0;
    }
  }
  __enable_irq();
}

void setup() {
  Serial.begin(9600);
}

void loop() {
  if (Serial.available() == 2) {
    currentNote = Serial.read();
    currentChannel = Serial.read();
    unsigned int freq = (float(440) * pow(2, float((currentNote - 57) / float(12))));
    Serial.println(currentNote);
    Serial.println(currentChannel);
    Serial.println(freq);
    tone_multi(currentChannel, freq, 0);
  }
}
