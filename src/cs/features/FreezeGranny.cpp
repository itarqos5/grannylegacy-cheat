#include "FreezeGranny.h"

namespace
{
    bool g_enabled = false;
}

bool feature::freeze::IsEnabled()
{
    return g_enabled;
}

void feature::freeze::SetEnabled(bool enabled)
{
    g_enabled = enabled;
}

void feature::freeze::Toggle()
{
    g_enabled = !g_enabled;
}
