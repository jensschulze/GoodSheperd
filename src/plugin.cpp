#include "plugin.hpp"

Plugin *pluginInstance;

void init(Plugin *p)
{
	pluginInstance = p;

	p->addModel(modelHurdle);
	p->addModel(modelSEQ3st);
	p->addModel(modelStable16);
}
