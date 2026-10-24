#include "IrCapture.h"
#include "Config.h"

#define IR_EDGE_BUFFER 128  // power of two

static volatile IrEdge edges[IR_EDGE_BUFFER];
static volatile uint8_t head, tail;
static volatile uint16_t overflowCount;
static volatile uint16_t overrunCount;

// Extends a Timer1 value read while interrupts are disabled to 32 bits. If an
// overflow is pending and the value is small, it was taken after the wrap.
static inline uint32_t extendTime(uint16_t ticks) {
  uint16_t high = overflowCount;
  if ((TIFR1 & _BV(TOV1)) && ticks < 0x8000)
    high++;
  return ((uint32_t)high << 16) | ticks;
}

static inline void pushEdge(uint32_t time, bool pulse) {
  uint8_t next = (head + 1) & (IR_EDGE_BUFFER - 1);
  if (next == tail) {
    overrunCount++;
    return;
  }
  edges[head].time = time;
  edges[head].pulse = pulse;
  head = next;
}

ISR(TIMER1_OVF_vect) {
  overflowCount++;
}

#if IR_INPUT == IR_INPUT_EXTINT

#if IR_EXTINT_NUM == 6
#define IR_PIN_DDR DDRE
#define IR_PIN_PORT PORTE
#define IR_PIN_IN PINE
#define IR_PIN_BIT PE6
#define IR_EICR EICRB
#define IR_ISC_SHIFT 4
#define IR_VECT INT6_vect
#else
#define IR_PIN_DDR DDRD
#define IR_PIN_PORT PORTD
#define IR_PIN_IN PIND
#define IR_PIN_BIT IR_EXTINT_NUM
#define IR_EICR EICRA
#define IR_ISC_SHIFT (IR_EXTINT_NUM * 2)
#if IR_EXTINT_NUM == 0
#define IR_VECT INT0_vect
#elif IR_EXTINT_NUM == 1
#define IR_VECT INT1_vect
#elif IR_EXTINT_NUM == 2
#define IR_VECT INT2_vect
#else
#define IR_VECT INT3_vect
#endif
#endif

ISR(IR_VECT) {
  uint32_t t = extendTime(TCNT1);
  // Receiver output is active low
  pushEdge(t, !(IR_PIN_IN & _BV(IR_PIN_BIT)));
}

static void beginInput() {
  IR_PIN_DDR &= ~_BV(IR_PIN_BIT);
  IR_PIN_PORT |= _BV(IR_PIN_BIT);  // pull-up
  EIMSK &= ~_BV(IR_EXTINT_NUM);
  IR_EICR = (IR_EICR & ~(3 << IR_ISC_SHIFT)) | (1 << IR_ISC_SHIFT);  // any edge
  EIFR = _BV(IR_EXTINT_NUM);
  EIMSK |= _BV(IR_EXTINT_NUM);
}

#else  // IR_INPUT_ANACOMP

// The pin is on the comparator's negative input and the 1.1 V bandgap on the
// positive input, so ACO is high while the receiver pulls the pin low (pulse).
// ICES1 selects which ACO edge the next capture fires on: rising = pulse start.

static inline bool comparatorPulse() {
  return ACSR & _BV(ACO);
}

static inline void armCapture(bool currentlyPulse) {
  if (currentlyPulse)
    TCCR1B &= ~_BV(ICES1);  // wait for pulse end
  else
    TCCR1B |= _BV(ICES1);   // wait for pulse start
  TIFR1 = _BV(ICF1);        // required after changing ICES1
}

ISR(TIMER1_CAPT_vect) {
  uint32_t t = extendTime(ICR1);
  bool pulse = TCCR1B & _BV(ICES1);
  pushEdge(t, pulse);
  armCapture(pulse);
  // If the input already flipped back while we were re-arming, that edge
  // would be missed. Record it now with a software timestamp.
  if (comparatorPulse() != pulse) {
    pushEdge(extendTime(TCNT1), !pulse);
    armCapture(!pulse);
  }
}

static void beginInput() {
  DDRF &= ~_BV(IR_ANACOMP_ADC);
  PORTF |= _BV(IR_ANACOMP_ADC);  // pull-up
  DIDR0 |= _BV(IR_ANACOMP_ADC);  // digital input buffer off

  ADCSRA &= ~_BV(ADEN);  // comparator mux needs the ADC off
  ADCSRB = (ADCSRB & ~_BV(MUX5)) | _BV(ACME);
  ADMUX = (ADMUX & ~0x1F) | IR_ANACOMP_ADC;
  ACSR = _BV(ACBG) | _BV(ACIC);
  delayMicroseconds(100);  // bandgap settling

  TCCR1B |= _BV(ICNC1);
  armCapture(comparatorPulse());
  TIMSK1 |= _BV(ICIE1);
}

#endif

namespace IrCapture {

void begin() {
  uint8_t sreg = SREG;
  cli();
  // Timer1 normal mode, clk/8 (overrides the core's PWM setup)
  TCCR1A = 0;
  TCCR1B = _BV(CS11);
  TCCR1C = 0;
  TCNT1 = 0;
  TIFR1 = 0xFF;
  TIMSK1 = _BV(TOIE1);
  beginInput();
  SREG = sreg;
}

bool pop(IrEdge &edge) {
  if (tail == head)
    return false;
  edge.time = edges[tail].time;
  edge.pulse = edges[tail].pulse;
  tail = (tail + 1) & (IR_EDGE_BUFFER - 1);
  return true;
}

uint32_t now() {
  uint8_t sreg = SREG;
  cli();
  uint32_t t = extendTime(TCNT1);
  SREG = sreg;
  return t;
}

uint16_t overruns() {
  uint8_t sreg = SREG;
  cli();
  uint16_t n = overrunCount;
  SREG = sreg;
  return n;
}

}  // namespace IrCapture
