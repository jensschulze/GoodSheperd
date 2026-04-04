#pragma once
#include "rack.hpp"
#include "componentlibrary.hpp"

using namespace rack;

// Forward-declare the Plugin, defined in GoodSheperd.cpp
extern Plugin *pluginInstance;

// Forward-declare each Model, defined in each module source file
extern Model *modelHurdle;
extern Model *modelSEQ3st;
extern Model *modelStable16;
extern Model *modelStall;
extern Model *modelSwitch1;
extern Model *modelSeqtrol;
extern Model *modelResrever;
extern Model *modelSpectral;
extern Model *modelRepool;
