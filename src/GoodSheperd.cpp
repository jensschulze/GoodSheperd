#include "GoodSheperd.hpp"

Plugin *pluginInstance;

void init(Plugin *p)
{
	pluginInstance = p;

	p->addModel(modelHurdle);
	p->addModel(modelSEQ3st);
	p->addModel(modelStable16);
	p->addModel(modelStumble);
}

// TODO: REFACTOR
NVGcolor prepareDisplay(NVGcontext *vg, Rect *box, int fontSize) {
	NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38); 
	NVGcolor borderColor = nvgRGB(0x68, 0x78, 0x98);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0.0, 0.0, box->size.x, box->size.y, 3.0);
	nvgFillColor(vg, backgroundColor);
	nvgFill(vg);
	nvgStrokeWidth(vg, 1.0);
	nvgStrokeColor(vg, borderColor);
	nvgStroke(vg);
	nvgFontSize(vg, fontSize);
	NVGcolor textColor = nvgRGB(0xc8, 0xe9, 0x4e); // afd22c

	return textColor;
}