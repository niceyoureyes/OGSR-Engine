#pragma once

#include "alife_space.h"
#include "hit_immunity.h"
#include "Hit.h"
#include "Level.h"

class CLevel;
class CWound;
class NET_Packet;
class CEntityAlive;
class CEntityCondition;

using namespace ALife;

struct SEntityConditionUI
{
    float passed_time;
    float accum_val;
};

struct SEntityConditionVal
{
    float cur;
    float min;
    float max;
    float speed;
    float deltas;
};

class CEntityConditionSimple
{
public:
    CEntityConditionSimple();
    virtual ~CEntityConditionSimple();

    IC float GetHealth() const { return m_fHealth; }
    IC float GetMaxHealth() const { return m_fHealthMax; }
    IC float& health() { return m_fHealth; }
    IC float& max_health() { return m_fHealthMax; }

private:
    float m_fHealth;
    float m_fHealthMax;
};

class CEntityCondition : public CEntityConditionSimple, public CHitImmunity
{
public:
    DEFINE_VECTOR(CWound*, WOUND_VECTOR, WOUND_VECTOR_IT);

    CEntityCondition(CEntityAlive* object);
    virtual ~CEntityCondition(void);
    virtual void reinit();

    /**************** BASE functions ****************/
    virtual void LoadCondition(LPCSTR section);
    virtual void save(NET_Packet& output_packet);
    virtual void load(IReader& input_packet);
    virtual void remove_links(const CObject* object);
    static void script_register(lua_State* L);

    float& par_value(LPCSTR name);
    void par_load(LPCSTR sect, LPCSTR prefix);
    SEntityConditionVal* mcondv() { return m_conds; }

    /**************** GET user functions ****************/

    //current
    float GetValue(ECondType val_type) const { return ((val_type == eCondTypeHealth) ? GetHealth() : m_conds[val_type].cur); }
    IC virtual float GetPower() const { return GetValue(eCondTypePower); }
    IC virtual float GetRadiation() const { return GetValue(eCondTypeRadiation); }
    IC virtual float GetPsyHealth() const { return GetValue(eCondTypePsyHealth); }
    IC virtual float GetAlcohol() const { return GetValue(eCondTypeAlcohol); }
    IC virtual float GetSatiety() const { return GetValue(eCondTypeSatiety); }
    IC virtual float GetThirst() const { return GetValue(eCondTypeThirst); }
    IC virtual float GetEntityMorale() const { return GetValue(eCondTypeMorale); }

    //max
    IC float GetMaxValue(ECondType val_type) const { return m_conds[val_type].max; }
    IC float GetMaxPower() const { return GetMaxValue(eCondTypePower); };

    /**************** GET LVAL user functions ****************/

    //current
    IC float& value(ECondType val_type) { return m_conds[val_type].cur; }
    IC float& power() { return value(eCondTypePower); }
    IC float& radiation() { return value(eCondTypeRadiation); }

    //max
    IC float& value_max(ECondType val_type) { return m_conds[val_type].max; }
    
    /**************** SET user functions ****************/

    //current
    void SetValue(ECondType val_type, float value) { ((val_type == eCondTypeHealth) ? health() : m_conds[val_type].cur) = value; }


    /**************** CHANGE user functions ****************/
    void ChangeValue( ECondType val_type, float value )
    {
        if (val_type == eCondTypeHealth && CanBeHarmed() == false)
            return;

        VERIFY(_valid(value));
        m_conds[val_type].deltas += value;
    }
    virtual void ChangeHealth(float value) { ChangeValue(eCondTypeHealth, value); }
    virtual void ChangePower(float value) { ChangeValue(eCondTypePower, value); }
    virtual void ChangeRadiation(float value) { ChangeValue(eCondTypeRadiation, value); }
    virtual void ChangePsyHealth(float value) { ChangeValue(eCondTypePsyHealth, value); }
    virtual void ChangeAlcohol(float value) { ChangeValue(eCondTypeAlcohol, value); };
    virtual void ChangeSatiety(float value) { ChangeValue(eCondTypeSatiety, value); };
    virtual void ChangeThirst(float value) { ChangeValue(eCondTypeThirst, value); };
    virtual void ChangeEntityMorale(float value) { ChangeValue(eCondTypeMorale, value); }
    void ChangeBleeding(float percent);

    /**************** OTHER user functions ****************/
    IC float GetHealthLost() const { return m_fHealthLost; }
    IC void SetMaxPower(float val) { clamp(val, 0.1f, 1.0f); m_conds[eCondTypePower].max = val; };
    virtual CWound* ConditionHit(SHit* pHDS);
    IC const float fdelta_time() const { return (m_fDeltaTime); }
    IC float& hit_bone_scale() { return (m_fHitBoneScale); }
    IC float& wound_bone_scale() { return (m_fWoundBoneScale); }

    /**************** WOUND user functions ****************/
    CWound* AddWound(float hit_power, ALife::EHitType hit_type, u16 element);
    IC const WOUND_VECTOR& wounds() const { return (m_WoundVector); }
    float BleedingSpeed();
    void ClearWounds();

    // Обновления состояния с течением времени
    virtual void UpdateCondition();
    void UpdateHealth();
    void UpdateWounds();
    void UpdateConditionTime();
    IC void SetConditionDeltaTime(float DeltaTime) { m_fDeltaTime = DeltaTime; };
    
    // WHO functions
    CObject* GetWhoHitLastTime() { return m_pWho; }
    u16 GetWhoHitLastTimeID() { return m_iWhoID; }

    // Can be harmed?
    IC void SetCanBeHarmedState(bool CanBeHarmed) { m_bCanBeHarmed = CanBeHarmed; }
    IC bool CanBeHarmed() const { return m_bCanBeHarmed; }

protected:
    // изменение силы хита в зависимости от надетого костюма (только для InventoryOwner)
    float HitOutfitEffect(float hit_power, ALife::EHitType hit_type, s16 element, float AP);
    // изменение потери сил в зависимости от надетого костюма
    float HitPowerEffect(float power_loss);

    // для подсчета состояния открытых ран, запоминается кость куда был нанесен хит и скорость потери крови из раны
    
    WOUND_VECTOR m_WoundVector;

    // величины
    bool m_conds_active[ALife::eCondTypeMax];
    SEntityConditionVal m_conds[ALife::eCondTypeMax];
    SEntityConditionUI m_conds_UI[ALife::eCondTypeMax];

    // величины кастом
    float m_fV_RadiationHealth;
    float m_fV_Bleeding;
    float m_fV_WoundIncarnation;
    float m_fMinWoundSize;
    bool m_bIsBleeding;

    // части хита, затрачиваемые на уменьшение здоровья и силы
    float m_fHealthHitPart[ALife::eHitTypeMax]{};
    float m_fPowerHitPart;

    // потеря здоровья и силы от последнего хита
    float m_fHealthLost;
    float m_fPowerLost;

    // для отслеживания времени
    u64 m_iLastTimeCalled;
    float m_fDeltaTime{};
    // кто нанес последний хит
    CObject* m_pWho;
    u16 m_iWhoID;

    // для передачи параметров из DamageManager
    float m_fHitBoneScale;
    float m_fWoundBoneScale;

    bool m_bTimeValid;
    bool m_bCanBeHarmed;

private:
    void reinit_deltas();
    void reinit_vals();
    void reinit_vals_UI();

    CEntityAlive* m_object;
};
