#pragma once
#include <app/SvgSwitch.hpp>
#include <asset.hpp>

using namespace rack;

extern Plugin *pluginInstance;

struct ArrowLeft : rack::app::SvgSwitch
{
    ArrowLeft()
    {
        momentary = true;
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ArrowLeft_0.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ArrowLeft_1.svg")));
    }
};

struct ArrowRight : rack::app::SvgSwitch
{
    ArrowRight()
    {
        momentary = true;
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ArrowRight_0.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ArrowRight_1.svg")));
    }
};
