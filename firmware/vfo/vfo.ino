// LARCset VFO — ESP32 + Si5351 + ST7735 1.44" TFT
//
// Libraries needed:
//   - Etherkit Si5351
//   - Adafruit ST7735 and ST7789 Library
//   - Adafruit GFX
//
// ─── Pinout ───────────────────────────────────────────────
//  Si5351
//    VCC → 3.3V
//    GND → GND
//    SDA → GPIO 21
//    SCL → GPIO 22
//
//  TFT (ST7735, 1.44" 128x128)
//    VCC → 3.3V
//    GND → GND
//    SCK → GPIO 18  (SPI default)
//    SDA → GPIO 23  (MOSI, SPI default)
//    CS  → GPIO 5
//    DC  → GPIO 4   (= A0 on some TFT boards)
//    RST → GPIO 13
//    BL  → 3.3V
//
//  PTT sense
//    signal → GPIO 33  (LOW = transmitting)
//    GND    → GND
//
//  Si5351 outputs
//    CLK0 → LPF → 100nF → LARCset T2 LO winding   (VFO)
//    CLK1 → not connected (disabled)
//    CLK2 → LPF → 100nF → LARCset T1/T3 BFO winding
//
//  WiFi: ESP32 hosts hotspot "LARCset-VFO" (no password)
//        Connect phone, open http://192.168.4.1
// ──────────────────────────────────────────────────────────

#include <Wire.h>
#include <SPI.h>
#include <si5351.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <WebServer.h>

// ---------------------------------------------------------------------------
// Pins
// ---------------------------------------------------------------------------
#define TFT_CS    5
#define TFT_DC    4
#define TFT_RST   13
#define PTT_PIN   33

// ---------------------------------------------------------------------------
// WiFi AP
// ---------------------------------------------------------------------------
const char* AP_SSID = "LARCset-VFO";   // no password — open network

WebServer server(80);

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define COL_BG        ST77XX_BLACK
#define COL_FREQ      ST77XX_GREEN
#define COL_TX        ST77XX_RED
#define COL_RX        ST77XX_CYAN
#define COL_BEAD_OFF  0x2104
#define COL_ROD       0x39E7
#define COL_SEP       0x1082
#define COL_LABEL   ST77XX_WHITE
#define COL_STEP    ST77XX_YELLOW

// ---------------------------------------------------------------------------
// Si5351
// ---------------------------------------------------------------------------
Si5351 si5351;

const int32_t SI5351_CORRECTION = 0;

// ---------------------------------------------------------------------------
// Frequency plan — 40m, high-side injection
// ---------------------------------------------------------------------------
const long IF_FREQ   = 11059000L;
const long BFO_FREQ  = 11057500L;
const long BAND_LOW  =  7000000L;
const long BAND_HIGH =  7300000L;
const long START_FREQ = 7055000L;

// ---------------------------------------------------------------------------
// Tuning state
// ---------------------------------------------------------------------------
long vfoFreq = START_FREQ;
long vfoStep = 1000;

// ---------------------------------------------------------------------------
// Si5351 helpers
// ---------------------------------------------------------------------------
void setVFO(long freq) {
  si5351.set_freq((long long)(IF_FREQ + freq) * 100LL, SI5351_CLK0);
}

void setBFO(long freq) {
  si5351.set_freq((long long)freq * 100LL, SI5351_CLK2);
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
void drawScreen(long freq, bool tx, bool fullRedraw) {
  if (fullRedraw) {
    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_LABEL);
    tft.setTextSize(1);
    tft.setCursor(2, 100);
    tft.print("Band: 40m LSB");
    tft.setCursor(2, 115);
    tft.print("Step:");
  }

  // RX / TX badge
  tft.fillRect(90, 2, 36, 18, COL_BG);
  tft.setTextSize(2);
  tft.setTextColor(tx ? COL_TX : COL_RX);
  tft.setCursor(90, 2);
  tft.print(tx ? "TX" : "RX");

  // Frequency — big, green, centred
  tft.fillRect(0, 30, 128, 36, COL_BG);
  tft.setTextSize(2);
  tft.setTextColor(COL_FREQ);
  char buf[16];
  sprintf(buf, "%ld.%03ld.%02ld", freq/1000000L, (freq%1000000L)/1000L, (freq%1000L)/10L);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(buf, 0, 30, &x1, &y1, &w, &h);
  tft.setCursor((128 - w) / 2, 38);
  tft.print(buf);

  // Step size
  tft.fillRect(40, 113, 86, 10, COL_BG);
  tft.setTextSize(1);
  tft.setTextColor(COL_STEP);
  tft.setCursor(40, 115);
  if      (vfoStep == 10)    tft.print("10 Hz");
  else if (vfoStep == 100)   tft.print("100 Hz");
  else if (vfoStep == 1000)  tft.print("1 kHz");
  else if (vfoStep == 10000) tft.print("10 kHz");
}

// ---------------------------------------------------------------------------
// Web UI
// ---------------------------------------------------------------------------
void handleRoot() {
  long mhz = vfoFreq / 1000000L;
  long khz = (vfoFreq % 1000000L) / 1000L;
  long hz  = vfoFreq % 1000L;
  char freqBuf[32];
  sprintf(freqBuf, "%ld.%03ld.%03ld", mhz, khz, hz);

  String html = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>LARCset VFO</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;}
body{background:#0a0a0a;color:#0f0;font-family:'Courier New',monospace;
     display:flex;flex-direction:column;align-items:center;
     min-height:100vh;padding:24px 16px;user-select:none;-webkit-user-select:none;}
#title{font-size:0.75em;color:#3a5a3a;letter-spacing:4px;text-transform:uppercase;margin-bottom:20px;}
#display{background:#060f06;border:1px solid #1a3a1a;border-radius:10px;
         padding:14px 28px;margin-bottom:28px;text-align:center;width:100%;max-width:320px;
         box-shadow:inset 0 0 30px rgba(0,255,0,0.04),0 0 20px rgba(0,200,0,0.08);}
#freq{font-size:2.6em;font-weight:bold;color:#00ff41;letter-spacing:3px;
      text-shadow:0 0 12px rgba(0,255,65,0.7);}
#sub{font-size:0.7em;color:#3a6a3a;letter-spacing:3px;margin-top:6px;}
#knob-wrap{touch-action:none;}
canvas{display:block;cursor:grab;}
canvas:active{cursor:grabbing;}
#steps{display:flex;gap:8px;margin-top:22px;}
.sb{font-family:'Courier New',monospace;font-size:0.75em;padding:7px 13px;
    background:#0d0d0d;color:#3a6a3a;border:1px solid #1e3a1e;border-radius:5px;cursor:pointer;}
.sb.on{background:#071407;color:#00ff41;border-color:#00cc33;
       box-shadow:0 0 8px rgba(0,255,65,0.25);}
</style>
</head>
<body>
<div id="title">LARCset &mdash; 40m LSB</div>
<div id="display">
  <div id="freq">)HTML" + String(freqBuf) + R"HTML(</div>
  <div id="sub">40m &bull; LSB &bull; RX</div>
</div>
<div id="knob-wrap">
  <canvas id="knob" width="280" height="280"></canvas>
</div>
<div id="steps">
  <button class="sb" onclick="setStep(10,this)">10 Hz</button>
  <button class="sb" onclick="setStep(100,this)">100 Hz</button>
  <button class="sb on" onclick="setStep(1000,this)">1 kHz</button>
  <button class="sb" onclick="setStep(10000,this)">10 kHz</button>
</div>
<script>
const cv=document.getElementById('knob');
const cx=cv.width/2,cy=cv.height/2,R=118;
let angle=0,dragging=false,lastA=null,acc=0;
const DEG=10;

function drawKnob(){
  const c=cv.getContext('2d');
  c.clearRect(0,0,cv.width,cv.height);

  // tick marks
  for(let i=0;i<60;i++){
    const a=(i/60)*Math.PI*2-Math.PI/2;
    const maj=i%5===0;
    const r1=R+5,r2=R+(maj?18:10);
    c.beginPath();
    c.moveTo(cx+Math.cos(a)*r1,cy+Math.sin(a)*r1);
    c.lineTo(cx+Math.cos(a)*r2,cy+Math.sin(a)*r2);
    c.strokeStyle=maj?'#2d5a2d':'#162816';
    c.lineWidth=maj?2:1;
    c.stroke();
  }

  // knob body
  const g=c.createRadialGradient(cx-35,cy-35,8,cx,cy,R);
  g.addColorStop(0,'#383838');
  g.addColorStop(0.45,'#1c1c1c');
  g.addColorStop(1,'#080808');
  c.beginPath();c.arc(cx,cy,R,0,Math.PI*2);
  c.fillStyle=g;c.fill();

  // knurling
  for(let i=0;i<72;i++){
    const a=(i/72)*Math.PI*2;
    c.beginPath();
    c.moveTo(cx+Math.cos(a)*(R-8),cy+Math.sin(a)*(R-8));
    c.lineTo(cx+Math.cos(a)*(R-1),cy+Math.sin(a)*(R-1));
    c.strokeStyle='#2a2a2a';c.lineWidth=1.5;c.stroke();
  }

  // rim
  c.beginPath();c.arc(cx,cy,R,0,Math.PI*2);
  c.strokeStyle='#303030';c.lineWidth=2;c.stroke();

  // indicator line
  const rad=(angle-90)*Math.PI/180;
  c.beginPath();
  c.moveTo(cx+Math.cos(rad)*30,cy+Math.sin(rad)*30);
  c.lineTo(cx+Math.cos(rad)*(R-12),cy+Math.sin(rad)*(R-12));
  c.strokeStyle='#00ff41';c.lineWidth=3;
  c.shadowColor='#00ff41';c.shadowBlur=14;c.stroke();c.shadowBlur=0;

  // indicator dot
  c.beginPath();
  c.arc(cx+Math.cos(rad)*(R-18),cy+Math.sin(rad)*(R-18),5,0,Math.PI*2);
  c.fillStyle='#00ff41';
  c.shadowColor='#00ff41';c.shadowBlur=18;c.fill();c.shadowBlur=0;

  // centre cap
  const g2=c.createRadialGradient(cx-8,cy-8,2,cx,cy,28);
  g2.addColorStop(0,'#2a2a2a');g2.addColorStop(1,'#0d0d0d');
  c.beginPath();c.arc(cx,cy,28,0,Math.PI*2);
  c.fillStyle=g2;c.fill();
  c.strokeStyle='#222';c.lineWidth=1;c.stroke();
}

function getAngle(e){
  const r=cv.getBoundingClientRect();
  const t=e.touches?e.touches[0]:e;
  return Math.atan2(t.clientY-r.top-cy,t.clientX-r.left-cx)*180/Math.PI;
}

let pending=0,timer=null;
function tune(n){
  pending+=n;
  if(timer)clearTimeout(timer);
  timer=setTimeout(()=>{
    if(!pending)return;
    fetch('/tune?n='+pending)
      .then(r=>r.text())
      .then(f=>{document.getElementById('freq').innerText=f;});
    pending=0;
  },40);
}

cv.addEventListener('mousedown',e=>{dragging=true;lastA=getAngle(e);acc=0;e.preventDefault();});
cv.addEventListener('mousemove',e=>{
  if(!dragging)return;e.preventDefault();
  const a=getAngle(e);let d=a-lastA;
  if(d>180)d-=360;if(d<-180)d+=360;
  lastA=a;angle+=d;acc+=d;drawKnob();
  while(acc>=DEG){acc-=DEG;tune(1);}
  while(acc<=-DEG){acc+=DEG;tune(-1);}
});
cv.addEventListener('mouseup',()=>{dragging=false;});
cv.addEventListener('touchstart',e=>{dragging=true;lastA=getAngle(e);acc=0;e.preventDefault();},{passive:false});
cv.addEventListener('touchmove',e=>{
  if(!dragging)return;e.preventDefault();
  const a=getAngle(e);let d=a-lastA;
  if(d>180)d-=360;if(d<-180)d+=360;
  lastA=a;angle+=d;acc+=d;drawKnob();
  while(acc>=DEG){acc-=DEG;tune(1);}
  while(acc<=-DEG){acc+=DEG;tune(-1);}
},{passive:false});
cv.addEventListener('touchend',()=>{dragging=false;});

function setStep(s,btn){
  fetch('/step?s='+s);
  document.querySelectorAll('.sb').forEach(b=>b.classList.remove('on'));
  btn.classList.add('on');
}

drawKnob();
</script>
</body>
</html>)HTML";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", html);
}

void handleTune() {
  if (server.hasArg("n")) {
    long n = server.arg("n").toInt();
    vfoFreq += n * vfoStep;
    if (vfoFreq < BAND_LOW)  vfoFreq = BAND_LOW;
    if (vfoFreq > BAND_HIGH) vfoFreq = BAND_HIGH;
    setVFO(vfoFreq);
    drawScreen(vfoFreq, false, false);
  }
  long mhz = vfoFreq / 1000000L;
  long khz = (vfoFreq % 1000000L) / 1000L;
  long hz  = vfoFreq % 1000L;
  char buf[32];
  sprintf(buf, "%ld.%03ld.%03ld", mhz, khz, hz);
  server.send(200, "text/plain", String(buf));
}

void handleStep() {
  if (server.hasArg("s")) {
    long s = server.arg("s").toInt();
    if (s == 10 || s == 100 || s == 1000 || s == 10000) {
      vfoStep = s;
      drawScreen(vfoFreq, false, false);
    }
  }
  server.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // TFT
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(1);
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_LABEL);
  tft.setTextSize(1);
  tft.setCursor(20, 55);
  tft.print("LARCset VFO");
  tft.setCursor(28, 68);
  tft.print("starting...");

  // I2C / Si5351
  Wire.begin();
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, SI5351_CORRECTION);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_4MA);
  si5351.output_enable(SI5351_CLK1, 0);
  setBFO(BFO_FREQ);
  setVFO(vfoFreq);

  // PTT
  pinMode(PTT_PIN, INPUT_PULLUP);

  // WiFi AP
  WiFi.softAP(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Web server routes
  server.on("/",      handleRoot);
  server.on("/tune",  handleTune);
  server.on("/step",  handleStep);
  server.begin();

  drawScreen(vfoFreq, false, true);
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
  server.handleClient();

  static bool lastPTT = false;
  bool ptt = (digitalRead(PTT_PIN) == LOW);
  if (ptt != lastPTT) {
    lastPTT = ptt;
    drawScreen(vfoFreq, ptt, false);
  }
}
