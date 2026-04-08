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

struct SEntityConditionVal
{
    float cur;
    float min;
    float max;
    float speed;
    float speed_total;
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
    float SetHealth(float val) { m_fHealth = val; }
    float SetMaxHealth(float val) { m_fHealthMax = val; }

private:
    float m_fHealth;
    float m_fHealthMax;
};

class CondScaler
{
public:
    typedef std::pair<ECondType, ECondType> cond_pair;
    typedef std::pair<std::vector<float>, std::vector<float>> scalers;
    typedef std::map<cond_pair, scalers> scale_store;
    CondScaler() {}
    virtual ~CondScaler() {};

    void Load(ECondType cond_from, ECondType cond_to, float cond_from_val_start, float cond_from_val_finish, std::vector<float> val_steps, std::vector<float> mult_steps)
    {
        if (val_steps.size() != mult_steps.size())
            return;

        std::sort(val_steps.begin(), val_steps.end());
        float val_step_min = val_steps.front();
        float val_step_range = val_steps.back() - val_steps.front();
        float cond_from_min = cond_from_val_start;
        float cond_from_range = cond_from_val_finish - cond_from_val_start;

        std::for_each(val_steps.begin(), val_steps.end(), [=](float& val) { val = (val - val_step_min) / val_step_range * cond_from_range + cond_from_min; });

        m_conds_scale[std::make_pair(cond_from, cond_to)] = std::make_pair(val_steps, mult_steps);
    }

    float Scale(ECondType cond_from, ECondType cond_to, float val)
    {
        return GetCurrScale(cond_from, cond_to, val).second * val;
    }

    std::pair<int, float> GetCurrScale(ECondType cond_from, ECondType cond_to, float cond_from_value)
    {
        scale_store::iterator it;

        if ((it = m_conds_scale.find(std::make_pair(cond_from, cond_to))) == m_conds_scale.end())
            return std::make_pair(-1, 1.f);

        scalers& sc = it->second;
        std::vector<float> &val_steps = sc.first;
        std::vector<float> &mult_steps = sc.second;
        
        if (cond_from_value < val_steps.front())
            return std::make_pair(0, mult_steps.front());

        for (size_t i = 0; i < val_steps.size() - 1; i++)
        {
            if ((fsimilar(val_steps[i], cond_from_value) || val_steps[i] <= cond_from_value) &&
                (fsimilar(cond_from_value, val_steps[i + 1]) || cond_from_value <= val_steps[i + 1]))
            {
                return std::make_pair(i, mult_steps[i] + (cond_from_value - val_steps[i]) * ((mult_steps[i + 1] - mult_steps[i]) / (val_steps[i + 1] - val_steps[i])));
            }
        }

        return std::make_pair(val_steps.size() - 1, mult_steps.back());
    }

private:
    scale_store m_conds_scale;
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

    /**************** GET user functions ****************/

    //current
    IC float GetValue(ECondType val_type)
    {
        return ((val_type == eCondTypeBleeding) ? BleedingSpeed() : GetValue_(val_type));
    }
    IC float GetValue_(ECondType val_type) const
    {
        return ((val_type == eCondTypeHealth) ? GetHealth() : m_conds[val_type].cur);
    }
    IC float GetPower() const { return GetValue_(eCondTypePower); }
    IC float GetBleeding() { return BleedingSpeed(); }
    IC float GetRadiation() const { return GetValue_(eCondTypeRadiation); }
    IC float GetPsyHealth() const { return GetValue_(eCondTypePsyHealth); }
    IC float GetAlcohol() const { return GetValue_(eCondTypeAlcohol); }
    IC float GetSatiety() const { return GetValue_(eCondTypeSatiety); }
    IC float GetThirst() const { return GetValue_(eCondTypeThirst); }
    IC float GetEntityMorale() const { return GetValue_(eCondTypeMorale); }

    //max
    IC float GetMaxValue(ECondType val_type) const
    {
        return ((val_type == eCondTypeHealth) ? GetMaxHealth() : m_conds[val_type].max);
    }
    IC float GetMaxPower() const { return GetMaxValue(eCondTypePower); };
    IC float GetMaxRadiation() const { return GetMaxValue(eCondTypeRadiation); }
    IC float GetMaxPsyHealth() const { return GetMaxValue(eCondTypePsyHealth); }
    IC float GetMaxAlcohol() const { return GetMaxValue(eCondTypeAlcohol); }
    IC float GetMaxSatiety() const { return GetMaxValue(eCondTypeSatiety); }
    IC float GetMaxThirst() const { return GetMaxValue(eCondTypeThirst); }
    IC float GetMaxEntityMorale() const { return GetMaxValue(eCondTypeMorale); }

    // speed
    IC float GetSpeedValue(ECondType val_type) const
    {
        return m_conds[val_type].speed;
    }
    IC float GetSpeedHealth() const { return GetSpeedValue(eCondTypeHealth); }
    IC float GetSpeedPower() const { return GetSpeedValue(eCondTypePower); }
    IC float GetSpeedBleeding() const { return GetSpeedValue(eCondTypeBleeding); }
    IC float GetSpeedRadiation() const { return GetSpeedValue(eCondTypeRadiation); }
    IC float GetSpeedPsyHealth() const { return GetSpeedValue(eCondTypePsyHealth); }
    IC float GetSpeedAlcohol() const { return GetSpeedValue(eCondTypeAlcohol); }
    IC float GetSpeedSatiety() const { return GetSpeedValue(eCondTypeSatiety); }
    IC float GetSpeedThirst() const { return GetSpeedValue(eCondTypeThirst); }
    IC float GetSpeedEntityMorale() const { return GetSpeedValue(eCondTypeMorale); }

    //speed total
    IC float GetSpeedTotalValue(ECondType val_type) const
    {
        return m_conds[val_type].speed_total;
    }
    IC float GetSpeedTotalHealth() const { return GetSpeedTotalValue(eCondTypeHealth); }
    IC float GetSpeedTotalPower() const { return GetSpeedTotalValue(eCondTypePower); };
    IC float GetSpeedTotalRadiation() const { return GetSpeedTotalValue(eCondTypeRadiation); }
    IC float GetSpeedTotalPsyHealth() const { return GetSpeedTotalValue(eCondTypePsyHealth); }
    IC float GetSpeedTotalAlcohol() const { return GetSpeedTotalValue(eCondTypeAlcohol); }
    IC float GetSpeedTotalSatiety() const { return GetSpeedTotalValue(eCondTypeSatiety); }
    IC float GetSpeedTotalThirst() const { return GetSpeedTotalValue(eCondTypeThirst); }
    IC float GetSpeedTotalEntityMorale() const { return GetSpeedTotalValue(eCondTypeMorale); }

    /**************** SET user functions ****************/

    //current
    void SetValue(ECondType val_type, float value)
    {
        if (val_type == eCondTypeHealth)
        {
            clamp(value, 0.f, GetMaxHealth());
            health() = value;
        }
        else
        {
            clamp(value, m_conds[val_type].min, m_conds[val_type].max);
            m_conds[val_type].cur = value;
        }
    }
    float SetPower(float value) { SetValue(eCondTypePower, value); }
    float SetRadiation(float value) { SetValue(eCondTypeRadiation, value); }
    float SetPsyHealth(float value) { SetValue(eCondTypePsyHealth, value); }
    float SetAlcohol(float value) { SetValue(eCondTypeAlcohol, value); }
    float SetSatiety(float value) { SetValue(eCondTypeSatiety, value); }
    float SetThirst(float value) { SetValue(eCondTypeThirst, value); }
    float SetEntityMorale(float value) { SetValue(eCondTypeMorale, value); }

    //max
    void SetMaxValue(ECondType val_type, float value)
    {
        if (val_type == eCondTypeHealth)
        {
            SetMaxHealth(value);
        }
        else
        {
            m_conds[val_type].max = value;
        }
    }
    void SetMaxPower(float value) { SetMaxValue(eCondTypePower, value); }
    void SetMaxRadiation(float value) { SetMaxValue(eCondTypeRadiation, value); }
    void SetMaxPsyHealth(float value) { SetMaxValue(eCondTypePsyHealth, value); }
    void SetMaxAlcohol(float value) { SetMaxValue(eCondTypeAlcohol, value); }
    void SetMaxSatiety(float value) { SetMaxValue(eCondTypeSatiety, value); }
    void SetMaxThirst(float value) { SetMaxValue(eCondTypeThirst, value); }
    void SetMaxEntityMorale(float value) { SetMaxValue(eCondTypeMorale, value); }

    //speed
    void SetSpeedValue(ECondType val_type, float value) { m_conds[val_type].speed = value; }
    void SetSpeedHealth(float value) { SetSpeedValue(eCondTypeHealth, value); }
    void SetSpeedPower(float value) { SetSpeedValue(eCondTypePower, value); }
    void SetSpeedBleeding(float value) { SetSpeedValue(eCondTypeBleeding, value); }
    void SetSpeedRadiation(float value) { SetSpeedValue(eCondTypeRadiation, value); }
    void SetSpeedPsyHealth(float value) { SetSpeedValue(eCondTypePsyHealth, value); }
    void SetSpeedAlcohol(float value) { SetSpeedValue(eCondTypeAlcohol, value); }
    void SetSpeedSatiety(float value) { SetSpeedValue(eCondTypeSatiety, value); }
    void SetSpeedThirst(float value) { SetSpeedValue(eCondTypeThirst, value); }
    void SetSpeedEntityMorale(float value) { SetSpeedValue(eCondTypeMorale, value); }

    /**************** CHANGE user functions ****************/

    //current and deltas
    void ChangeCurrValue(ECondType val_type)
    {
        if (val_type == eCondTypeHealth)
        {
            health() += m_conds[eCondTypeHealth].deltas;
            clamp(health(), m_conds[eCondTypeHealth].min, m_conds[eCondTypeHealth].max);
        }
        else if (val_type == eCondTypeBleeding)
        {
            ChangeBleeding(m_conds[eCondTypeBleeding].deltas);
        }
        else
        {
            m_conds[val_type].cur += m_conds[val_type].deltas;
            clamp(m_conds[val_type].cur, m_conds[val_type].min, m_conds[val_type].max);
        }

        m_conds[val_type].deltas = 0;
        m_conds[val_type].speed_total = 0;
    }
    void ChangeValue(ECondType val_type, float value)
    {
        if (val_type == eCondTypeHealth && CanBeHarmed() == false)
            return;

        VERIFY(_valid(value));
        m_conds[val_type].deltas += value;
    }
    void ChangeHealth(float value) { ChangeValue(eCondTypeHealth, value); }
    void ChangePower(float value) { ChangeValue(eCondTypePower, value); }
    void ChangeRadiation(float value) { ChangeValue(eCondTypeRadiation, value); }
    void ChangePsyHealth(float value) { ChangeValue(eCondTypePsyHealth, value); }
    void ChangeAlcohol(float value) { ChangeValue(eCondTypeAlcohol, value); };
    void ChangeSatiety(float value) { ChangeValue(eCondTypeSatiety, value); };
    void ChangeThirst(float value) { ChangeValue(eCondTypeThirst, value); };
    void ChangeEntityMorale(float value) { ChangeValue(eCondTypeMorale, value); }
    void ChangeBleeding(float percent);

    //total speed
    void ChangeSpeedTotalValue(ECondType val_type, float value)
    {
        m_conds[val_type].speed_total += value;
    }
    void ChangeSpeedTotalHealth(float value) { ChangeSpeedTotalValue(eCondTypeHealth, value); }
    void ChangeSpeedTotalPower(float value) { ChangeSpeedTotalValue(eCondTypePower, value); }
    void ChangeSpeedTotalRadiation(float value) { ChangeSpeedTotalValue(eCondTypeRadiation, value); }
    void ChangeSpeedTotalPsyHealth(float value) { ChangeSpeedTotalValue(eCondTypePsyHealth, value); }
    void ChangeSpeedTotalAlcohol(float value) { ChangeSpeedTotalValue(eCondTypeAlcohol, value); };
    void ChangeSpeedTotalSatiety(float value) { ChangeSpeedTotalValue(eCondTypeSatiety, value); };
    void ChangeSpeedTotalThirst(float value) { ChangeSpeedTotalValue(eCondTypeThirst, value); };
    void ChangeSpeedTotalEntityMorale(float value) { ChangeSpeedTotalValue(eCondTypeMorale, value); }
    
    /**************** GETREL user functions ****************/

    float GetRel(ECondType from_type, ECondType to_type)
    {
        return m_conds_rel[std::make_pair(from_type, to_type)];
    }
    float GetRel_RadiationHealth() { return GetRel(eCondTypeRadiation, eCondTypeHealth); }
    float GetRel_BleedingHealth() { return GetRel(eCondTypeBleeding, eCondTypeHealth); }
    float GetRel_SatietyHealth() { return GetRel(eCondTypeSatiety, eCondTypeHealth); }
    float GetRel_ThirstHealth() { return GetRel(eCondTypeThirst, eCondTypeHealth); }
    float GetRel_RadiationPower() { return GetRel(eCondTypeRadiation, eCondTypePower); }
    float GetRel_BleedingPower() { return GetRel(eCondTypeBleeding, eCondTypePower); }
    float GetRel_SatietyPower() { return GetRel(eCondTypeSatiety, eCondTypePower); }
    float GetRel_ThirstPower() { return GetRel(eCondTypeThirst, eCondTypePower); }

    /**************** SETREL user functions ****************/

    void SetRel(ECondType from_type, ECondType to_type, float value)
    {
        m_conds_rel[std::make_pair(from_type, to_type)] = value;
    }
    void SetRel_RadiationHealth(float value) { SetRel(eCondTypeRadiation, eCondTypeHealth, value); }
    void SetRel_BleedingHealth(float value) { SetRel(eCondTypeBleeding, eCondTypeHealth, value); }
    void SetRel_SatietyHealth(float value) { SetRel(eCondTypeSatiety, eCondTypeHealth, value); }
    void SetRel_ThirstHealth(float value) { SetRel(eCondTypeThirst, eCondTypeHealth, value); }
    void SetRel_RadiationPower(float value) { SetRel(eCondTypeRadiation, eCondTypePower, value); }
    void SetRel_BleedingPower(float value) { SetRel(eCondTypeBleeding, eCondTypePower, value); }
    void SetRel_SatietyPower(float value) { SetRel(eCondTypeSatiety, eCondTypePower, value); }
    void SetRel_ThirstPower(float value) { SetRel(eCondTypeThirst, eCondTypePower, value); }

    /**************** OTHER user functions ****************/
    IC float GetHealthLost() const { return m_fHealthLost; }
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
    IC void SetConditionDeltaTime(float DeltaTime) { m_fDeltaTime = DeltaTime; };
    void UpdateConditionTime();
    virtual void UpdateCondition();
    void UpdateHealth();
    void UpdatePower();
    void UpdateWounds();
    
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
    std::map<std::pair<ECondType, ECondType>, float> m_conds_rel;
    CondScaler m_scaler;

    // величины кастом
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

    CEntityAlive* m_object;
};
