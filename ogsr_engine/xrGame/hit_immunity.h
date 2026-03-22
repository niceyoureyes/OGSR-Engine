// hit_immunity.h: класс для тех объектов, которые поддерживают
//				   коэффициенты иммунитета для разных типов хитов
//////////////////////////////////////////////////////////////////////

#pragma once

#include "alife_space.h"
#include "script_export_space.h"
#include "hit_immunity_space.h"

class CHitImmunity
{
public:
    CHitImmunity();
    virtual ~CHitImmunity();

    // user calls
    float GetHitImmunity(ALife::EHitType hit_type) const { return m_HitTypeK[hit_type]; }
    virtual float AffectHit(float power, ALife::EHitType hit_type);
    HitImmunity::HitTypeSVec& immunities() { return m_HitTypeK; }

    static void script_register(lua_State* L);
    virtual void LoadImmunities(LPCSTR section, CInifile* ini);
    virtual CHitImmunity* cast_hit_immunities() { return this; }

protected:
    HitImmunity::HitTypeSVec m_HitTypeK;
};
