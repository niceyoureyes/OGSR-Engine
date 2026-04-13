// ActorCondition.h: класс состояния игрока

#pragma once

#include "EntityCondition.h"
#include "actor_defs.h"
#include "..\xr_3da\feel_touch.h"

template <typename _return_type>
class CScriptCallbackEx;

class CActor;

class CActorCondition : public CEntityCondition
{
    friend class CScriptActor;

public:
    typedef CEntityCondition inherited;

    enum
    {
        eCriticalPowerReached     = (1 << 0),
        eCriticalMaxPowerReached  = (1 << 1),
        eCriticalBleedingSpeed    = (1 << 2),
        eCriticalSatietyReached   = (1 << 3),
        eCriticalRadiationReached = (1 << 4),
        eWeaponJammedReached      = (1 << 5),
        ePhyHealthMinReached      = (1 << 6),
        eCantWalkWeight           = (1 << 7),
        eLimping                  = (1 << 8),
        eCantWalk                 = (1 << 9),
        eCantSprint               = (1 << 10),
        eCriticalThirstReached    = (1 << 11),
    };

    CActorCondition(CActor* object);
    virtual ~CActorCondition(void);
    IC CActor& object() const { VERIFY(m_object); return (*m_object); }
    virtual void reinit();

    /**************** BASE functions ****************/
    virtual void LoadCondition(LPCSTR section);
    virtual void save(NET_Packet& output_packet);
    virtual void load(IReader& input_packet);

    /**************** OTHER user functions ****************/
    virtual CWound* ConditionHit(SHit* pHDS);
    void PowerHit(float power, bool apply_outfit);
    float HitSlowmo(SHit* pHDS);

    // Обновления состояния с течением времени
    virtual void UpdateCondition();

    // хромание при потере сил и здоровья
    virtual bool IsLimping();
    virtual bool IsCantWalk();
    virtual bool IsCantWalkWeight();
    virtual bool IsCantSprint();
    virtual bool IsCantJump(float weight);

    void ConditionStand(float weight);
    void ConditionWalk(float weight, bool accel, bool sprint);
    void ConditionJump(float weight);

    void SetMaxWalkWeight(float _weight) { m_MaxWalkWeight = _weight; }

    bool DisableSprint(SHit* pHDS);

    // новое непонятное
    void AffectDamage_InjuriousMaterialAndMonstersInfluence();
    float GetInjuriousMaterialDamage();

    void net_Relcase(CObject* O);
    void set_monsters_aura_radius(float r)
    {
        if (r > monsters_aura_radius)
            monsters_aura_radius = r;
    };

    float m_MaxWalkWeight;
    Flags16 m_condition_flags;

protected:
    float m_fPowerLeakSpeed;
    float m_fJumpPower;
    float m_fStandPower;
    float m_fWalkPower;
    float m_fJumpWeightPower;
    float m_fWalkWeightPower;
    float m_fOverweightWalkK;
    float m_fOverweightJumpK;
    float m_fAccelK;
    float m_fSprintK;

    bool m_bJumpRequirePower;

    float m_f_time_affected;

    //порог силы и здоровья меньше которого актер начинает хромать
    float m_fLimpingPowerBegin;
    float m_fLimpingPowerEnd;
    float m_fCantWalkPowerBegin;
    float m_fCantWalkPowerEnd;

    float m_fCantSprintPowerBegin;
    float m_fCantSprintPowerEnd;

    float m_fLimpingHealthBegin;
    float m_fLimpingHealthEnd;

protected:
    Feel::Touch* monsters_feel_touch;
    float monsters_aura_radius;

private:
    CActor* m_object;
    void UpdateTutorialThresholds();
};
