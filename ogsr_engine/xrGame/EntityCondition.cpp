#include "stdafx.h"
#include "entitycondition.h"
#include "inventoryowner.h"
#include "customoutfit.h"
#include "inventory.h"
#include "wound.h"
#include "level.h"
#include "game_cl_base.h"
#include "entity_alive.h"
#include "../Include/xrRender/Kinematics.h"
#include "object_broker.h"

#define MAX_HEALTH 1.0f
#define MIN_HEALTH -0.01f

#define MAX_POWER 1.0f
#define MIN_POWER 0.0f

#define MAX_RADIATION 1.0f
#define MIN_RADIATION 0.0f

#define MAX_PSY_HEALTH 1.0f
#define MIN_PSY_HEALTH 0.0f

CEntityConditionSimple::CEntityConditionSimple()
{
    max_health() = MAX_HEALTH;
    health() = MAX_HEALTH;
}

CEntityConditionSimple::~CEntityConditionSimple() {}

CEntityCondition::CEntityCondition(CEntityAlive* object) : CEntityConditionSimple()
{
    VERIFY(object);
    m_object = object;

    m_conds_active[eCondTypeHealth] = 1;
    m_conds_active[eCondTypePower] = 1;
    m_conds_active[eCondTypeBleeding] = 1;
    m_conds_active[eCondTypeRadiation] = 1;
    m_conds_active[eCondTypePsyHealth] = 1;
    m_conds_active[eCondTypeMorale] = 1;

    m_bCanBeHarmed = true;
    m_fMinWoundSize = 0.00001f;
    m_fHitBoneScale = 1.f;
    m_fWoundBoneScale = 1.f;
    m_fPowerHitPart = 0.5f;

    reinit();
}

CEntityCondition::~CEntityCondition(void)
{ 
    ClearWounds();
}

void CEntityCondition::reinit_deltas()
{
    for (size_t i = 0; i < eCondTypeMax; i++)
    {
        m_conds[i].deltas = 0;
    }
}

void CEntityCondition::reinit_vals()
{
    m_conds[eCondTypeHealth].cur = MAX_HEALTH; health() = MAX_HEALTH;
    m_conds[eCondTypeHealth].min = MIN_HEALTH;
    m_conds[eCondTypeHealth].max = MAX_HEALTH; max_health() = MAX_HEALTH;

    m_conds[eCondTypePower].cur = MAX_POWER;
    m_conds[eCondTypePower].min = MIN_POWER;
    m_conds[eCondTypePower].max = MAX_POWER;

    m_conds[eCondTypeRadiation].cur = MIN_RADIATION;
    m_conds[eCondTypeRadiation].min = MIN_RADIATION;
    m_conds[eCondTypeRadiation].max = MAX_RADIATION;

    m_conds[eCondTypePsyHealth].cur = MAX_PSY_HEALTH;
    m_conds[eCondTypePsyHealth].min = MIN_PSY_HEALTH;
    m_conds[eCondTypePsyHealth].max = MAX_PSY_HEALTH;

    m_conds[eCondTypeAlcohol].cur = 0.0;
    m_conds[eCondTypeAlcohol].min = 0.0;
    m_conds[eCondTypeAlcohol].max = 1.0;

    m_conds[eCondTypeSatiety].cur = 1.0;
    m_conds[eCondTypeSatiety].min = 0.0;
    m_conds[eCondTypeSatiety].max = 1.0;

    m_conds[eCondTypeThirst].cur = 1.0;
    m_conds[eCondTypeThirst].min = 0.0;
    m_conds[eCondTypeThirst].max = 1.0;

    m_conds[eCondTypeMorale].cur = 1.f;
    m_conds[eCondTypeMorale].min = 0.0;
    m_conds[eCondTypeMorale].max = 1.f;
}

void CEntityCondition::reinit()
{
    m_iLastTimeCalled = 0;
    m_bTimeValid = false;
    m_fHealthLost = 0.f;
    m_fPowerLost = 0.f;
    m_pWho = NULL;
    m_iWhoID = NULL;

    reinit_vals();
    reinit_deltas();
    ClearWounds();
}

void CEntityCondition::LoadCondition(LPCSTR entity_section)
{
    LPCSTR section = READ_IF_EXISTS(pSettings, r_string, entity_section, "condition_sect", entity_section);

    par_load(section, "");
    m_conds[eCondTypeRadiation].speed *= (-1);

    m_fMinWoundSize = pSettings->r_float(section, "min_wound_size");
    m_fPowerHitPart = pSettings->r_float(section, "power_hit_part");
    float fHealthHitPart = READ_IF_EXISTS(pSettings, r_float, section, "health_hit_part", 1.f);

    for (int hit_type = 0; hit_type < (int)ALife::eHitTypeMax; ++hit_type)
    {
        LPCSTR hit_name = ALife::g_cafHitType2String((ALife::EHitType)hit_type);
        xr_string s("health_");
        s += hit_name;
        s += "_hit_part";
        m_fHealthHitPart[hit_type] = READ_IF_EXISTS(pSettings, r_float, section, s.c_str(), fHealthHitPart);
    }

    m_scaler.Load(eCondTypeRadiation, eCondTypeHealth, m_conds[eCondTypeRadiation].min, m_conds[eCondTypeRadiation].max, {0, 1, 3, 7, 15}, {1, 2.5, 10, 80, 2000});
}

void CEntityCondition::ChangeBleeding(float percent)
{
    //затянуть раны
    for (WOUND_VECTOR_IT it = m_WoundVector.begin(); m_WoundVector.end() != it; ++it)
    {
        (*it)->Incarnation(percent, m_fMinWoundSize);
        if (0 == (*it)->TotalSize())
            (*it)->SetDestroy(true);
    }
}

void CEntityCondition::ClearWounds()
{
    for (WOUND_VECTOR_IT it = m_WoundVector.begin(); m_WoundVector.end() != it; ++it)
        xr_delete(*it);
    m_WoundVector.clear();

    m_bIsBleeding = false;
}

bool RemoveWoundPred(CWound* pWound)
{
    if (pWound->GetDestroy())
    {
        xr_delete(pWound);
        return true;
    }
    return false;
}

void CEntityCondition::UpdateWounds()
{
    //убрать все зашившие раны из списка
    m_WoundVector.erase(std::remove_if(m_WoundVector.begin(), m_WoundVector.end(), &RemoveWoundPred), m_WoundVector.end());
}

void CEntityCondition::UpdateConditionTime()
{
    u64 _cur_time = Level().GetGameTime();

    if (m_bTimeValid)
    {
        if (_cur_time > m_iLastTimeCalled)
        {
            float x = float(_cur_time - m_iLastTimeCalled) / 1000.0f;
            SetConditionDeltaTime(x);
        }
        else
        {
            SetConditionDeltaTime(0.0f);
        }
    }
    else
    {
        SetConditionDeltaTime(0.0f);
        m_bTimeValid = true;

        reinit_deltas();
    }

    m_iLastTimeCalled = _cur_time;
}

//вычисление параметров с ходом игрового времени
void CEntityCondition::UpdateCondition()
{
    if (GetHealth() <= 0)
        return;

    if (m_conds_active[eCondTypeHealth])
        UpdateHealth();

    if (m_conds_active[eCondTypePower])
        UpdatePower();

    if (m_conds_active[eCondTypeBleeding])
    {
        m_bIsBleeding = fis_zero(BleedingSpeed()) ? false : true;
        UpdateWounds();
    }

    for (size_t i = 0; i < eCondTypeMax; i++)
    {
        if (m_conds_active[i] == false)
            continue;

        ECondType etype = static_cast<ECondType>(i);

        // Custom speed + Base speed
        ChangeSpeedTotalValue(etype, GetSpeedValue(etype));

        // Immediate delta + Time delta
        m_conds[etype].speed_total_UI = GetSpeedTotalValue(etype);
        ChangeValue(etype, m_fDeltaTime * GetSpeedTotalValue(etype));

        // Final update and clear
        ChangeCurrValue(etype);
    }
}

float CEntityCondition::HitOutfitEffect(float hit_power, ALife::EHitType hit_type, s16 element, float AP)
{
    CInventoryOwner* pInvOwner = smart_cast<CInventoryOwner*>(m_object);
    if (!pInvOwner)
        return hit_power;

    CCustomOutfit* pOutfit = (CCustomOutfit*)pInvOwner->inventory().m_slots[OUTFIT_SLOT].m_pIItem;
    if (!pOutfit)
        return hit_power;

    float new_hit_power = hit_power;

    if (hit_type == ALife::eHitTypeFireWound)
        new_hit_power = pOutfit->HitThruArmour(hit_power, element, AP);
    else
        new_hit_power *= pOutfit->GetHitTypeProtection(hit_type, element);

    //увеличить изношенность костюма
    pOutfit->Hit(hit_power, hit_type);

    return new_hit_power;
}

float CEntityCondition::HitPowerEffect(float power_loss)
{
    CInventoryOwner* pInvOwner = smart_cast<CInventoryOwner*>(m_object);
    if (!pInvOwner)
        return power_loss;

    CCustomOutfit* pOutfit = (CCustomOutfit*)pInvOwner->inventory().m_slots[OUTFIT_SLOT].m_pIItem;
    if (!pOutfit)
        return power_loss;

    float new_power_loss = power_loss * pOutfit->GetPowerLoss();

    return new_power_loss;
}

CWound* CEntityCondition::AddWound(float hit_power, ALife::EHitType hit_type, u16 element)
{
    //максимальное число косточек 64
    VERIFY(element < 64 || BI_NONE == element);

    //запомнить кость по которой ударили и силу удара
    WOUND_VECTOR_IT it = m_WoundVector.begin();
    for (; it != m_WoundVector.end(); ++it)
    {
        if ((*it)->GetBoneNum() == element)
            break;
    }

    CWound* pWound = NULL;

    //новая рана
    if (it == m_WoundVector.end())
    {
        pWound = xr_new<CWound>(element);
        pWound->AddHit(hit_power * ::Random.randF(0.5f, 1.5f), hit_type);
        m_WoundVector.push_back(pWound);
    }
    //старая
    else
    {
        pWound = *it;
        pWound->AddHit(hit_power * ::Random.randF(0.5f, 1.5f), hit_type);
    }

    VERIFY(pWound);
    return pWound;
}

CWound* CEntityCondition::ConditionHit(SHit* pHDS)
{
    //кто нанес последний хит
    m_pWho = pHDS->who;
    m_iWhoID = (NULL != pHDS->who) ? pHDS->who->ID() : 0;

    float hit_power_org = pHDS->damage();
    float hit_power = HitOutfitEffect(hit_power_org, pHDS->hit_type, pHDS->boneID, pHDS->ap);
    bool bAddWound = true;

    switch (pHDS->hit_type)
    {
    case ALife::eHitTypeRadiation:
        ChangeRadiation(hit_power);
        return NULL;
        break;

    case ALife::eHitTypeTelepatic:
        hit_power *= m_HitTypeK[pHDS->hit_type];
        ChangePsyHealth(-hit_power);
        bAddWound = false;
        break;

    case ALife::eHitTypeChemicalBurn:
        hit_power *= m_HitTypeK[pHDS->hit_type];
        break;

    case ALife::eHitTypeShock:
        hit_power *= m_HitTypeK[pHDS->hit_type];
        m_fHealthLost = hit_power * m_fHealthHitPart[pHDS->hit_type];
        m_fPowerLost = hit_power * m_fPowerHitPart;
        ChangeHealth(-m_fHealthLost);
        ChangePower(-m_fPowerLost);
        bAddWound = false;
        break;

    case ALife::eHitTypeExplosion:
    case ALife::eHitTypeStrike:
    case ALife::eHitTypePhysicStrike:
        hit_power *= m_HitTypeK[pHDS->hit_type];
        m_fHealthLost = hit_power * m_fHealthHitPart[pHDS->hit_type];
        m_fPowerLost = hit_power * m_fPowerHitPart;
        ChangeHealth(-m_fHealthLost);
        ChangePower(-m_fPowerLost);
        break;

    case ALife::eHitTypeBurn:
    case ALife::eHitTypeFireWound:
    case ALife::eHitTypeWound:
        hit_power *= m_HitTypeK[pHDS->hit_type];
        m_fHealthLost = hit_power * m_fHealthHitPart[pHDS->hit_type] * m_fHitBoneScale;
        m_fPowerLost = hit_power * m_fPowerHitPart;
        ChangeHealth(-m_fHealthLost);
        ChangePower(-m_fPowerLost);
        break;

    default:
        { R_ASSERT2(0, "unknown hit type"); }
        break;
    }

    if (bDebug)
        Msg("%s hitted in %s with %f[%f]", m_object->Name(), smart_cast<IKinematics*>(m_object->Visual())->LL_BoneName(pHDS->boneID), m_fHealthLost * 100.0f, hit_power_org);

    //раны добавляются только живому
    if (bAddWound && GetHealth() > 0)
        return AddWound(hit_power * m_fWoundBoneScale, pHDS->hit_type, pHDS->boneID);
    else
        return NULL;
}

float CEntityCondition::BleedingSpeed()
{
    float bleeding_speed = 0;

    for (WOUND_VECTOR_IT it = m_WoundVector.begin(); m_WoundVector.end() != it; ++it)
        bleeding_speed += (*it)->TotalSize();

    return (m_WoundVector.empty() ? 0.f : bleeding_speed / m_WoundVector.size());
}

void CEntityCondition::UpdateHealth()
{
    float rad_roat_speed = m_scaler.Scale(eCondTypeRadiation, eCondTypeHealth, GetRadiation()) * GetRel_RadiationHealth();
    float bleeding_speed = m_scaler.Scale(eCondTypeBleeding, eCondTypeHealth, BleedingSpeed()) * GetRel_BleedingHealth();
    float starve_speed = m_scaler.Scale(eCondTypeSatiety, eCondTypeHealth, GetSatiety()) * GetRel_SatietyHealth();
    float dry_speed = m_scaler.Scale(eCondTypeThirst, eCondTypeHealth, GetThirst()) * GetRel_ThirstHealth();

    ChangeSpeedTotalValue(eCondTypeHealth, -1.f * (rad_roat_speed + bleeding_speed + starve_speed + dry_speed));
}

void CEntityCondition::UpdatePower()
{
    float rad_roat_speed = m_scaler.Scale(eCondTypeRadiation, eCondTypePower, GetRadiation()) * GetRel_RadiationPower();
    float bleeding_speed = m_scaler.Scale(eCondTypeBleeding, eCondTypePower, BleedingSpeed()) * GetRel_BleedingPower();
    float starve_speed = m_scaler.Scale(eCondTypeSatiety, eCondTypePower, GetSatiety()) * GetRel_SatietyPower();
    float dry_speed = m_scaler.Scale(eCondTypeThirst, eCondTypePower, GetThirst()) * GetRel_ThirstPower();
    
    ChangeSpeedTotalValue(eCondTypePower, GetSpeedPower() / (rad_roat_speed + bleeding_speed + starve_speed + dry_speed));
}

void CEntityCondition::save(NET_Packet& output_packet)
{
    u8 is_alive = (GetHealth() > 0.f) ? 1 : 0;

    output_packet.w_u8(is_alive);
    if (is_alive)
    {
        save_data(m_conds[eCondTypePower].cur, output_packet);
        save_data(m_conds[eCondTypeRadiation].cur, output_packet);
        save_data(m_conds[eCondTypeMorale].cur, output_packet);
        save_data(m_conds[eCondTypePsyHealth].cur, output_packet);

        output_packet.w_u8((u8)m_WoundVector.size());
        for (WOUND_VECTOR_IT it = m_WoundVector.begin(); m_WoundVector.end() != it; it++)
            (*it)->save(output_packet);
    }
}

void CEntityCondition::load(IReader& input_packet)
{
    m_bTimeValid = false;

    u8 is_alive = input_packet.r_u8();
    if (is_alive)
    {
        load_data(m_conds[eCondTypePower].cur, input_packet);
        load_data(m_conds[eCondTypeRadiation].cur, input_packet);
        load_data(m_conds[eCondTypeMorale].cur, input_packet);
        load_data(m_conds[eCondTypePsyHealth].cur, input_packet);

        ClearWounds();

        m_WoundVector.resize(input_packet.r_u8());

        if (!m_WoundVector.empty())
        {
            for (u32 i = 0; i < m_WoundVector.size(); i++)
            {
                CWound* pWound = xr_new<CWound>(BI_NONE);
                pWound->load(input_packet);
                m_WoundVector[i] = pWound;
            }
        }
    }
}

const static size_t CCV_NAMES_COUNT = 7;
constexpr LPCSTR CCV_NAMES[] = {"radiation_v", "radiation_health_v", "morale_v", "psy_health_v", "bleeding_v", "wound_incarnation_v", "health_restore_v"};

float& CEntityCondition::par_value(LPCSTR name)
{
    float* values[] = { &m_conds[eCondTypeRadiation].speed
                      , &m_conds_rel[std::make_pair(eCondTypeRadiation, eCondTypeHealth)]
                      , &m_conds[eCondTypeMorale].speed
                      , &m_conds[eCondTypePsyHealth].speed
                      , &m_conds_rel[std::make_pair(eCondTypeBleeding, eCondTypeHealth)]
                      , &m_conds[eCondTypeBleeding].speed
                      , &m_conds[eCondTypeHealth].speed };

    for (int i = 0; i < CCV_NAMES_COUNT; i++)
        if (strstr(name, CCV_NAMES[i]))
            return *values[i];

    static float fake = 0;
    return fake;
}

void CEntityCondition::par_load(LPCSTR sect, LPCSTR prefix)
{
    string256 str;

    for (int i = 0; i < CCV_NAMES_COUNT; i++)
    {
        strconcat(sizeof(str), str, CCV_NAMES[i], prefix);
        float v = READ_IF_EXISTS(pSettings, r_float, sect, str, 0.0f);
        par_value(CCV_NAMES[i]) = v;
    }
}

void CEntityCondition::remove_links(const CObject* object)
{
    if (m_pWho != object)
        return;

    m_pWho = m_object;
    m_iWhoID = m_object->ID();
}

/************* Script *************/

using namespace luabind;

bool get_entity_crouch(CEntity::SEntityState* S) { return S->bCrouch; }
bool get_entity_fall(CEntity::SEntityState* S) { return S->bFall; }
bool get_entity_jump(CEntity::SEntityState* S) { return S->bJump; }
bool get_entity_sprint(CEntity::SEntityState* S) { return S->bSprint; }

void CEntityCondition::script_register(lua_State* L)
{
    module(L)[class_<CEntity::SEntityState>("SEntityState")
                  .property("crouch", &get_entity_crouch)
                  .property("fall", &get_entity_fall)
                  .property("jump", &get_entity_jump)
                  .property("sprint", &get_entity_sprint)
                  .def_readonly("velocity", &CEntity::SEntityState::fVelocity)
                  .def_readonly("a_velocity", &CEntity::SEntityState::fAVelocity),

              class_<CEntityCondition>("CEntityCondition")
                  .def("fdelta_time", &CEntityCondition::fdelta_time)
                  .def_readonly("has_valid_time", &CEntityCondition::m_bTimeValid)
                  .property("health", &CEntityCondition::GetHealth, &CEntityCondition::SetHealth)
                  .property("max_health", &CEntityCondition::GetMaxHealth, &CEntityCondition::SetMaxHealth)
                  .property("power", &CEntityCondition::GetPower, &CEntityCondition::SetPower)
                  .property("power_max", &CEntityCondition::GetMaxPower, &CEntityCondition::SetMaxPower)
                  .property("psy_health", &CEntityCondition::GetPsyHealth, &CEntityCondition::SetPsyHealth)
                  .property("psy_health_max", &CEntityCondition::GetMaxPsyHealth, &CEntityCondition::SetMaxPsyHealth)
                  .property("radiation", &CEntityCondition::GetRadiation, &CEntityCondition::SetRadiation)
                  .property("radiation_max", &CEntityCondition::GetMaxRadiation, &CEntityCondition::SetMaxRadiation)
                  .property("morale", &CEntityCondition::GetEntityMorale, &CEntityCondition::SetEntityMorale)
                  .property("morale_max", &CEntityCondition::GetMaxEntityMorale, &CEntityCondition::SetMaxEntityMorale)
                  .def_readonly("is_bleeding", &CEntityCondition::m_bIsBleeding)
                  .def_readwrite("min_wound_size", &CEntityCondition::m_fMinWoundSize)
                  .def_readwrite("power_hit_part", &CEntityCondition::m_fPowerHitPart)];
}
