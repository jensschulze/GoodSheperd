#pragma once

#include <rack.hpp>

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

struct ArrowUp : rack::app::SvgSwitch
{
    ArrowUp()
    {
        momentary = true;
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ArrowUp_0.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ArrowUp_1.svg")));
    }
};

struct ArrowDown : rack::app::SvgSwitch
{
    ArrowDown()
    {
        momentary = true;
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ArrowDown_0.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ArrowDown_1.svg")));
    }
};

struct SquareSwitch : rack::app::SvgSwitch
{
    SquareSwitch()
    {
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/SquareSwitch_0.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/SquareSwitch_1.svg")));
    }
};
