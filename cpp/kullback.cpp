#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/console.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <cstdio>


static const double MARGIN_LEFT   = 40.0;
static const double MARGIN_RIGHT  = 20.0;
static const double MARGIN_TOP    = 20.0;
static const double MARGIN_BOTTOM = 40.0;


extern "C" {
EM_JS(void, clearCanvas, (), {
    var canvas = document.getElementById('graphCanvas');
    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);
});

EM_JS(void, drawLine, (double x1, double y1, double x2, double y2, const char* color, double width, double opacity), {
    var canvas = document.getElementById('graphCanvas');
    var ctx = canvas.getContext('2d');
    ctx.globalAlpha = opacity;
    ctx.beginPath();
    ctx.strokeStyle = UTF8ToString(color);
    ctx.lineWidth = width;
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();
    ctx.globalAlpha = 1.0;
});

EM_JS(void, drawGraph, (int* x, int* y, const char** colors, int points, double xAxis, double width, double opacity), {
    var canvas = document.getElementById('graphCanvas');
    var ctx = canvas.getContext('2d');
    ctx.globalAlpha = opacity;
    ctx.lineWidth = width;
    ctx.lineJoin = 'round';
    ctx.lineCap  = 'round';
    var xVals = new Int32Array(Module.HEAP32.buffer, x, points);
    var yVals = new Int32Array(Module.HEAP32.buffer, y, points);
    var controlPoints = {};
    const bezierAmount = points;
    for(let i = 0; i < points; i++) {
        let X = xVals[i];
        let Y = yVals[i];
        let prevX = xVals[i-1];
        let prevY = yVals[i-1];
        let nextX = xVals[i+1];
        let nextY = yVals[i+1];

        let distToPrev = Math.sqrt((X-prevX)**2+(Y-prevY)**2);
        let distToNext = Math.sqrt((X-nextX)**2+(Y-nextY)**2);

        let slopeBetweenNeighbors = (nextY - prevY)/(nextX - prevX);
        if(isNaN(slopeBetweenNeighbors)){
            if(i == 0) {
                slopeBetweenNeighbors = (nextY - Y)/(nextX - X);
            }
            else if(i == points-1) {
                slopeBetweenNeighbors = (prevY - Y)/(prevX - X);
            }
        }
        let pointInfo = {left: [X-distToPrev/bezierAmount, Math.min(Y-distToPrev*slopeBetweenNeighbors/bezierAmount, xAxis)], right: [X+distToNext/bezierAmount, Math.min(Y+distToNext*slopeBetweenNeighbors/bezierAmount, xAxis)]};
        controlPoints[i] = pointInfo;
    }
    if(points >= 2) {
        for (let i = 1; i < points; i++) {
            ctx.beginPath();
            ctx.moveTo(xVals[i-1], yVals[i-1]);
            ctx.strokeStyle = UTF8ToString(getValue(colors + (i-1) * 4, "i32"));
            if(i > 0 && i < points) {
                ctx.bezierCurveTo(controlPoints[i-1].right[0], controlPoints[i-1].right[1], controlPoints[i].left[0], controlPoints[i].left[1], xVals[i], yVals[i]);
            }
            else {
                ctx.lineTo(xVals[i],yVals[i]);
            }
            ctx.stroke();
        }
    }
    ctx.globalAlpha = 1.0;
});

EM_JS(void, fillCircle, (double cx, double cy, double radius, const char* color), {
    var canvas = document.getElementById('graphCanvas');
    var ctx = canvas.getContext('2d');
    ctx.beginPath();
    ctx.fillStyle = UTF8ToString(color);
    ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
    ctx.fill();
});


EM_JS(void, drawText, (const char* text, double x, double y, const char* color, double fontSize, double angleDeg, const char* align), {
    var canvas = document.getElementById('graphCanvas');
    var ctx = canvas.getContext('2d');
    ctx.save();
    ctx.fillStyle = UTF8ToString(color);
    ctx.font = fontSize + 'px "Fira Code", monospace';
    ctx.textAlign = UTF8ToString(align);
    ctx.translate(x, y);
    var rad = angleDeg * Math.PI / 180.0;
    ctx.rotate(rad);
    ctx.fillText(UTF8ToString(text), 0, 0);
    ctx.restore();
});


EM_JS(void, drawLabelBox, (const char* txt, double x, double y), {
    var canvas = document.getElementById('graphCanvas');
    var ctx = canvas.getContext('2d');
    ctx.save();
    var label = UTF8ToString(txt).split('\n');
    ctx.font = '12px "Fira Code", monospace';
    var padding = 6;
    var textW = 0;
    for (let i = 0; i < label.length; i++) {
        let w = ctx.measureText(label[i]).width;
        if (w > textW) {
            textW = w;
        }
    }
    var textH = 14 * label.length;

    var offsetX = 8;
    var offsetY = 8;
    var boxX = x + offsetX;
    var boxY = y - textH - offsetY - 2 * padding;

    if (boxX + textW + 2 * padding > canvas.width) {
      boxX = x - textW - 2 * padding - offsetX;
    }

    if (boxY < 0) {
      boxY = y + offsetY;
    }
    if (boxX < 0) boxX = 0;

    ctx.roundRect(boxX, boxY, textW + 2 * padding, textH + 2 * padding, 5);
    ctx.fill();
    ctx.fillStyle = 'black';
    ctx.textBaseline = 'top';

    for (let i = 0; i < label.length; i++) {
            ctx.fillText(label[i], boxX + padding, boxY + padding + 2 + (i*14) );
    }
    ctx.restore();
});
}


static std::vector<double> g_xvals;  
static std::vector<double> g_yvals; 
static std::vector<double> g_scaledX;
static std::vector<double> g_scaledY; 
static std::vector<bool>   g_alwaysLabels; 
static double g_minY  = 0.0;
static double g_maxY  = 0.0;
static int    g_rnge  = 2;          
static bool   g_valid = false; 


double ioc(const std::vector<unsigned char>& data) {
    if (data.size() < 2) return 0.0;
    long long freq[256];
    std::fill(freq, freq + 256, 0);
    for (auto c : data) {
        freq[c]++;
    }
    long long numerator = 0;
    for (int i = 0; i < 256; i++) {
        long long x = freq[i];
        numerator += x * (x - 1);
    }
    double denom = (double)data.size() * (data.size() - 1);
    return (denom > 0.0) ? (numerator / denom) : 0.0;
}

std::vector<std::vector<unsigned char>> transpose(const std::vector<unsigned char>& txt, int n) {
    std::vector<std::vector<unsigned char>> blocks(n);
    for (size_t i = 0; i < txt.size(); i++) {
        blocks[i % n].push_back(txt[i]);
    }
    return blocks;
}


double scaleXval(double xVal, double canvasWidth) {
    double domainMinX = 1.0;
    double domainMaxX = (double)(g_rnge - 1);
    double rangeMinX  = MARGIN_LEFT;
    double rangeMaxX  = canvasWidth - MARGIN_RIGHT;
    double domainSize = domainMaxX - domainMinX;
    if (domainSize < 1e-12) domainSize = 1.0;
    double t = (xVal - domainMinX) / domainSize;
    return rangeMinX + t * (rangeMaxX - rangeMinX);
}

double scaleYval(double yVal, double canvasHeight) {
    double domainMinY = g_minY;
    double domainMaxY = g_maxY;
    double rangeMinY  = canvasHeight - MARGIN_BOTTOM; 
    double rangeMaxY  = MARGIN_TOP; 
    double domainSize = domainMaxY - domainMinY;
    if (domainSize < 1e-12) domainSize = 1.0;
    double t = (yVal - domainMinY) / domainSize;
    return rangeMinY - t * (rangeMinY - rangeMaxY);
}


double distPointToSegment(double Mx, double My,
                          double Ax, double Ay,
                          double Bx, double By)
{
    double ABx = Bx - Ax;
    double ABy = By - Ay;
    double AMx = Mx - Ax;
    double AMy = My - Ay;
    double ab2 = ABx * ABx + ABy * ABy;
    double am_ab = AMx * ABx + AMy * ABy;
    double t = (ab2 > 1e-12) ? (am_ab / ab2) : 0.0;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double Nx = Ax + t * ABx;
    double Ny = Ay + t * ABy;
    double dx = Nx - Mx;
    double dy = Ny - My;
    return std::sqrt(dx * dx + dy * dy);
}


void redrawPlot(int highlightLine, int highlightPoint, int showLabelPoint, double canvasWidth, double canvasHeight) {
    clearCanvas();
    if (!g_valid || g_scaledX.empty() || g_scaledY.empty()) {
        return;
    }
    double xAxisY = canvasHeight - MARGIN_BOTTOM;
    double yAxisX = MARGIN_LEFT;
    drawLine(yAxisX, xAxisY, canvasWidth - MARGIN_RIGHT, xAxisY, "white", 1.5, 1);
    drawLine(yAxisX, xAxisY, yAxisX, MARGIN_TOP, "white", 1.5, 1);

    std::vector<std::string> colors = {"#EE72F1", "#AA60FF", "#23B0FF", "#78E2A0", "#DBD963", "#FFA86A", "#FF6266"};
    int mod_amnt =  g_rnge/10;
    if(mod_amnt < 1) {
        mod_amnt = 1;
    }
    int j = 2;
    for (int i = 2; i < g_rnge; i++) {
        if(i % mod_amnt == 0){
                double cx = scaleXval((double)i, canvasWidth);
            drawLine(cx, xAxisY - 3, cx, xAxisY + 3, "#bbb", 1.0, 1);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%d", i);
            drawText(buf, cx, xAxisY + 15, colors[(j-2) % 7].c_str(), 12, 0.0, "center");
            j++;
        }
    }

    std::vector<int> x_points = {};
    std::vector<int> y_points = {};
    std::vector<const char *> lineColors = {};
    for (int i = 0; i < (int)g_scaledX.size(); i++) {
        x_points.push_back(g_scaledX[i]);
        y_points.push_back(g_scaledY[i]);
        if (i == highlightLine) {
            lineColors.push_back("#23B0FF");
        }
        else {
            lineColors.push_back("#AA60FF");
        }
    }

    const char* colorsArray[x_points.size()];
    int i = 0;
    for(auto c : lineColors) {
        colorsArray[i] = c;
        i++;
    }

    drawGraph(x_points.data(), y_points.data(), colorsArray, x_points.size(), xAxisY, 2.0, 1);

    drawLine(yAxisX, xAxisY, canvasWidth - MARGIN_RIGHT, xAxisY, "white", 1.5, .65);
    drawLine(yAxisX, xAxisY, yAxisX, MARGIN_TOP, "white", 1.5, .65);

    if (highlightPoint >= 0 && highlightPoint < (int)g_scaledX.size()) {
        fillCircle(g_scaledX[highlightPoint], g_scaledY[highlightPoint], 5.0, "#23B0FF");
    }
    else if (highlightLine >= 0 && highlightPoint == -1) {
        fillCircle(g_scaledX[highlightLine], g_scaledY[highlightLine], 5.0, "#23B0FF");
        fillCircle(g_scaledX[highlightLine+1], g_scaledY[highlightLine+1], 5.0, "#23B0FF");
    }

    if (showLabelPoint >= 0 && showLabelPoint < (int)g_scaledX.size()) {
        int idx = showLabelPoint;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "length = %.0f\nioc = %.4f", g_xvals[idx], g_yvals[idx]);
        drawLabelBox(buf, g_scaledX[idx], g_scaledY[idx]);
    }

    for (int i = 0; i < (int)g_alwaysLabels.size(); i++) {
        if (g_alwaysLabels[i]) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.0f", g_xvals[i]);
            double labelY = g_scaledY[i] - 10;
            double labelX = g_scaledX[i];
            if (labelY < MARGIN_TOP) {
                labelY = g_scaledY[i];
                labelX -= 8;
            }
            drawText(buf, labelX, labelY, "#EE72F1", 18, 0.0, "center");
        }
    }

    drawText("transposition length", (canvasWidth / 2.0), canvasHeight - 5, "#aaa", 14, 0.0, "center");
    drawText("ioc", (MARGIN_LEFT / 1.5), (canvasHeight / 2) - 5,  "#aaa", 14, -90.0, "center");
}

static inline bool is_base64(unsigned char c) {
  return (std::isalnum(c) || c == '+' || c == '/');
}

std::vector<unsigned char> base64_decode(const std::string &encoded_string) {
  static const std::string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";
  int in_len = encoded_string.size();
  int i = 0, in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::vector<unsigned char> ret;
  while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i == 4) {
      for (i = 0; i < 4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);
      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
      for (i = 0; i < 3; i++)
        ret.push_back(char_array_3[i]);
      i = 0;
    }
  }
  if (i) {
    for (int j = i; j < 4; j++)
      char_array_4[j] = 0;
    for (int j = 0; j < 4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);
    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
    for (int j = 0; j < i - 1; j++) ret.push_back(char_array_3[j]);
  }
  return ret;
}

bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

bool isBase64Char(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           (c == '+') ||
           (c == '/') ||
           (c == '=');
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
void runKullbackTest(const char* inputBytes, double threshold, int maxKeyLength, int alwaysOn, double canvasWidth, double canvasHeight, const char* dataType, int inputLength, int ignoreErrors) {
    g_valid = false;
    g_xvals.clear();
    g_yvals.clear();
    g_scaledX.clear();
    g_scaledY.clear();
    g_alwaysLabels.clear();

    std::string type(dataType);
    std::vector<unsigned char> raw;
    if (type == "utf-8") {
        raw.assign(inputBytes, inputBytes + std::strlen(inputBytes));
    } else if (type == "hex" || type == "file") {
        for (int i = 0; i + 1 < inputLength; i += 2) {
            if ((!isHexDigit(inputBytes[i]) || !isHexDigit(inputBytes[i+1])) && !ignoreErrors) {
                clearCanvas();
                drawText("invalid input", (canvasWidth / 2.0) - 30, (canvasHeight / 2.0), "#FFA86A", 32, 0.0, "center");
                return;
            }
            char hexByte[3] = { inputBytes[i], inputBytes[i+1], '\0' };
            unsigned int byte;
            std::sscanf(hexByte, "%x", &byte);
            raw.push_back(static_cast<unsigned char>(byte));
        }
    } else if (type == "base64") {
        for (int i = 0; i < inputLength; i++) {
            if (!isBase64Char(inputBytes[i]) && !ignoreErrors) {
                clearCanvas();
                drawText("invalid input", (canvasWidth / 2.0) - 30, (canvasHeight / 2.0), "#FFA86A", 32, 0.0, "center");
                return;
            }
        }
        raw = base64_decode(std::string(inputBytes, inputLength));
    } else if (type == "binary") {
        for (int i = 0; i < inputLength; i += 8) {
            unsigned char byte = 0;
            for (int j = 0; j < 8 && (i + j) < inputLength; j++) {
                byte <<= 1;
                if (inputBytes[i+j] == '1') {
                    byte |= 1;
                } else if (inputBytes[i+j] != '0' && !ignoreErrors) {
                    clearCanvas();
                    drawText("invalid input", (canvasWidth / 2.0) - 30, (canvasHeight / 2.0), "#FFA86A", 32, 0.0, "center");
                    return;
                }
            }
            raw.push_back(byte);
        }
    } else {
        raw.assign(inputBytes, inputBytes + std::strlen(inputBytes));
    }
    if (raw.size() < 2) {
        clearCanvas();
        return;
    }

    int halfMinus = (int)(raw.size() / 2) - 1;
    if (halfMinus < 2) halfMinus = 2;
    int r = std::min(halfMinus, maxKeyLength);
    if (r < 2) r = 2;
    g_rnge = r;
    int count = r - 1;
    g_xvals.resize(count);
    g_yvals.resize(count);
    double minY = 1e9;
    double maxY = -1e9;
    for (int i = 1; i < r; i++) {
        auto blocks = transpose(raw, i);
        double sum = 0.0;
        for (auto &b : blocks) {
            sum += ioc(b);
        }
        double avg = sum / blocks.size();
        int idx = i - 1;
        g_xvals[idx] = (double)i;
        g_yvals[idx] = avg;
        if (avg < minY) minY = avg;
        if (avg > maxY) maxY = avg;
    }
    if (minY == maxY) { minY -= 0.5; maxY += 0.5; }
    g_minY = minY;
    g_maxY = maxY;

    g_scaledX.resize(count);
    g_scaledY.resize(count);
    for (int i = 0; i < count; i++) {
        g_scaledX[i] = scaleXval(g_xvals[i], canvasWidth);
        g_scaledY[i] = scaleYval(g_yvals[i], canvasHeight);
    }
    g_alwaysLabels.resize(count, false);
    if (alwaysOn) {
        double sum = std::accumulate(g_yvals.begin(), g_yvals.end(), 0.0);
        double mean = sum / count;
        double accum = 0.0;
        for (double v : g_yvals) {
            accum += (v - mean) * (v - mean);
        }
        double stddev = (count > 1) ? std::sqrt(accum / (count - 1)) : 0.0;
        for (int i = 0; i < count; i++) {
            if (stddev > 1e-9 && ((g_yvals[i] - mean) / stddev) > threshold) {
                g_alwaysLabels[i] = true;
            }
        }
    }
    g_valid = true;
    redrawPlot(-1, -1, -1, canvasWidth, canvasHeight);
}

EMSCRIPTEN_KEEPALIVE
void highlightAt(double mx, double my, double canvasWidth, double canvasHeight) {
    if (!g_valid) {
        // clearCanvas();
        return;
    }

    const double pointRadius = 15.0;
    int bestPt = -1;
    double bestPtDist = 1e9;
    for (int i = 0; i < (int)g_scaledX.size(); i++) {
        double dx = g_scaledX[i] - mx;
        double dy = g_scaledY[i] - my;
        double dist = std::sqrt(dx*dx + dy*dy);
        if (dist < bestPtDist) {
            bestPtDist = dist;
            bestPt = i;
        }
    }
    bool nearPoint = (bestPtDist <= pointRadius);

    const double lineRadius = 15.0;
    int bestLine = -1;
    double bestLineDist = 1e9;
    for (int i = 0; i < (int)g_scaledX.size() - 1; i++) {
        double dist = distPointToSegment(mx, my,
                                         g_scaledX[i],   g_scaledY[i],
                                         g_scaledX[i+1], g_scaledY[i+1]);
        if (dist < bestLineDist) {
            bestLineDist = dist;
            bestLine = i;
        }
    }
    bool nearLine = (bestLineDist <= lineRadius);

    if (nearPoint) {
        redrawPlot(-1, bestPt, bestPt, canvasWidth, canvasHeight);
    } else if (nearLine) {
        redrawPlot(bestLine, -1, -1, canvasWidth, canvasHeight);
    } else {
        redrawPlot(-1, -1, -1, canvasWidth, canvasHeight);
    }
}
}