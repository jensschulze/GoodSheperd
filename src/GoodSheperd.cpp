#include "GoodSheperd.hpp"

Plugin *plugin;

void init(Plugin *p)
{
	plugin = p;

	p->addModel(modelHurdle);
	p->addModel(modelSEQ3st);
	p->addModel(modelStable16);
}
