/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Entities/Pet.h"
#include "Database/DatabaseEnv.h"
#include "Log/Log.h"
#include "Server/WorldPacket.h"
#include "Globals/ObjectMgr.h"
#include "Spells/SpellMgr.h"
#include "Tools/Formulas.h"
#include "Spells/SpellAuras.h"
#include "Entities/Unit.h"

Pet::Pet(PetType type) :
    Creature(CREATURE_SUBTYPE_PET),
    m_resetTalentsCost(0), m_resetTalentsTime(0), m_usedTalentCount(0),
    m_removed(false),  m_petType(type), m_duration(0),
    m_bonusdamage(0), m_auraUpdateMask(0), m_loading(false),
    m_declinedname(nullptr), m_petModeFlags(PET_MODE_DEFAULT), m_originalCharminfo(nullptr)
{
    m_name = "Pet";
    m_regenTimer = 4000;
    m_holyPowerRegenTimer = REGEN_TIME_HOLY_POWER;

    // pets always have a charminfo, even if they are not actually charmed
    CharmInfo* charmInfo = InitCharmInfo(this);

    if (type == MINI_PET)                                   // always passive
        charmInfo->SetReactState(REACT_PASSIVE);
}

Pet::~Pet()
{
    delete m_originalCharminfo;
    delete m_declinedname;
}

void Pet::AddToWorld()
{
    ///- Register the pet for guid lookup
    if (!IsInWorld())
        GetMap()->GetObjectsStore().insert<Pet>(GetObjectGuid(), (Pet*)this);

    Unit::AddToWorld();
}

void Pet::RemoveFromWorld()
{
    ///- Remove the pet from the accessor
    if (IsInWorld())
        GetMap()->GetObjectsStore().erase<Pet>(GetObjectGuid(), (Pet*)nullptr);

    ///- Don't call the function for Creature, normal mobs + totems go in a different storage
    Unit::RemoveFromWorld();
}

SpellCastResult Pet::TryLoadFromDB(Unit* owner, uint32 petentry /*= 0*/, uint32 petnumber /*= 0*/, bool current /*= false*/, PetType mandatoryPetType /*= MAX_PET_TYPE*/)
{
    uint32 ownerid = owner->GetGUIDLow();

    std::ostringstream request;
    //                 0      1          2               3
    request << "SELECT entry, curhealth, CreatedBySpell, PetType ";

    if (petnumber)
        // known petnumber entry
        request << "FROM character_pet WHERE owner = '" << ownerid << "' AND id = '" << petnumber << "'";
    else if (current)
        // current pet (slot 0)
        request << "FROM character_pet WHERE owner = '" << ownerid << "' AND slot = '" << uint32(PET_SAVE_AS_CURRENT) << "'";
    else if (petentry)
        // known petentry entry (unique for summoned pet, but non unique for hunter pet (only from current or not stabled pets)
        request << "FROM character_pet WHERE owner = '" << ownerid << "' AND entry = '" << petentry << "' AND (slot = '" << uint32(PET_SAVE_AS_CURRENT) << "' OR slot > '" << uint32(PET_SAVE_LAST_STABLE_SLOT) << "')";
    else
        // any current or other non-stabled pet (for hunter "call pet")
        request << "FROM character_pet WHERE owner = '" << ownerid << "' AND (slot = '" << uint32(PET_SAVE_AS_CURRENT) << "' OR slot > '" << uint32(PET_SAVE_LAST_STABLE_SLOT) << "')";

    QueryResult* result = CharacterDatabase.PQuery("%s", request.str().c_str());

    if (!result)
        return SPELL_FAILED_NO_PET;

    Field* fields = result->Fetch();

    petentry = fields[0].GetUInt32();
    uint32 savedHealth = fields[1].GetUInt32();
    uint32 summon_spell_id = fields[2].GetUInt32();
    PetType petType = PetType(fields[3].GetUInt8());

    delete result;

    // update for case of current pet "slot = 0"
    if (!petentry)
        return SPELL_FAILED_NO_PET;

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        sLog.outError("Pet entry %u does not exist but used at pet load (owner: %s).", petentry, owner->GetGuidStr().c_str());
        return SPELL_FAILED_NO_PET;
    }

    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(summon_spell_id);

    bool isTemporarySummoned = spellInfo && GetSpellDuration(spellInfo) > 0;

    // check temporary summoned pets like mage water elemental
    if (current && isTemporarySummoned)
        return SPELL_FAILED_NO_PET;

    Player const* ownerPlayer = nullptr;
    if (owner->GetTypeId() == TYPEID_PLAYER)
        ownerPlayer = static_cast<Player const*>(owner);

    if (petType == HUNTER_PET && !creatureInfo->isTameable(ownerPlayer ? ownerPlayer->CanTameExoticPets() : false))
        return SPELL_FAILED_NO_PET;

    if (!savedHealth)
        return SPELL_FAILED_TARGETS_DEAD;

    if (mandatoryPetType != MAX_PET_TYPE && petType != mandatoryPetType)
        return SPELL_FAILED_BAD_TARGETS;

    return SPELL_CAST_OK; // If errors occur down the line, one must think about data consistency
}

bool Pet::LoadPetFromDB(Player* owner, uint32 petentry /*= 0*/, uint32 petnumber /*= 0*/, bool current /*= false*/, uint32 healthPercentage /*= 0*/, bool permanentOnly /*= false*/, bool forced /*= false*/)
{
    m_loading = true;

    uint32 ownerid = owner->GetGUIDLow();

    QueryResult* result;

    if (petnumber)
        // known petnumber entry                  0   1      2(?)   3        4      5    6           7     8     9        10         11       12      13        14                 15                 16              17
        result = CharacterDatabase.PQuery("SELECT id, entry, owner, modelid, level, exp, Reactstate, slot, name, renamed, curhealth, curmana, abdata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType "
            "FROM character_pet WHERE owner = '%u' AND id = '%u'",
            ownerid, petnumber);
    else if (current)
        // current pet (slot 0)                   0   1      2(?)   3        4      5    6           7     8     9        10         11       12      13        14                 15                 16              17
        result = CharacterDatabase.PQuery("SELECT id, entry, owner, modelid, level, exp, Reactstate, slot, name, renamed, curhealth, curmana, abdata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType "
            "FROM character_pet WHERE owner = '%u' AND slot = '%u'",
            ownerid, PET_SAVE_AS_CURRENT );
    else if (petentry)
        // known petentry entry (unique for summoned pet, but non unique for hunter pet (only from current or not stabled pets)
        //                                        0   1      2(?)   3        4      5    6           7     8     9        10         11       12      13        14                 15                 16              17
        result = CharacterDatabase.PQuery("SELECT id, entry, owner, modelid, level, exp, Reactstate, slot, name, renamed, curhealth, curmana, abdata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType "
            "FROM character_pet WHERE owner = '%u' AND entry = '%u' AND (slot = '%u' OR slot > '%u') ",
            ownerid, petentry,PET_SAVE_AS_CURRENT,PET_SAVE_LAST_STABLE_SLOT);
    else
        // any current or other non-stabled pet (for hunter "call pet")
        //                                        0   1      2(?)   3        4      5    6           7     8     9        10         11       12      13        14                 15                 16              17
        result = CharacterDatabase.PQuery("SELECT id, entry, owner, modelid, level, exp, Reactstate, slot, name, renamed, curhealth, curmana, abdata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType "
            "FROM character_pet WHERE owner = '%u' AND (slot = '%u' OR slot > '%u') ",
            ownerid,PET_SAVE_AS_CURRENT,PET_SAVE_LAST_STABLE_SLOT);

    if (!result)
        return false;

    Field* fields = result->Fetch();

    // update for case of current pet "slot = 0"
    petentry = fields[1].GetUInt32();
    if (!petentry)
    {
        delete result;
        return false;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        sLog.outError("Pet entry %u does not exist but used at pet load (owner: %s).", petentry, owner->GetGuidStr().c_str());
        delete result;
        return false;
    }

    uint32 summon_spell_id = fields[16].GetUInt32();
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(summon_spell_id);

    if (permanentOnly && spellInfo && GetSpellDuration(spellInfo) > 0)
    {
        delete result;
        return false;
    }

    PetType pet_type = PetType(fields[17].GetUInt8());
    if (pet_type == HUNTER_PET)
    {
        if (!creatureInfo->isTameable(owner->CanTameExoticPets()))
        {
            delete result;
            return false;
        }
    }

    m_petType = pet_type;

    uint32 pet_number = fields[0].GetUInt32();

    if (!forced && owner->IsPetNeedBeTemporaryUnsummoned())
    {
        // set temporary summon that way its possible if the player unmount to resummon it automaticaly
        owner->SetTemporaryUnsummonedPetNumber(pet_number);

        // change pet slot directly in database
        CharacterDatabase.BeginTransaction();
        static SqlStatementID ChangePetSlot_ID;
        SqlStatement ChangePetSlot = CharacterDatabase.CreateStatement(ChangePetSlot_ID, "UPDATE character_pet SET slot = ? WHERE id = ? ");
        ChangePetSlot.PExecute(uint32(PET_SAVE_AS_CURRENT), pet_number);
        CharacterDatabase.CommitTransaction();

        delete result;
        return false;
    }
    else
        owner->SetTemporaryUnsummonedPetNumber(0);

    Map* map = owner->GetMap();

    CreatureCreatePos pos(owner, owner->GetOrientation(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

    uint32 guid = pos.GetMap()->GenerateLocalLowGuid(HIGHGUID_PET);
    if (!Create(guid, pos, creatureInfo, pet_number))
    {
        delete result;
        return false;
    }

    setPetType(pet_type);
    setFaction(owner->GetFaction());
    SetUInt32Value(UNIT_CREATED_BY_SPELL, summon_spell_id);

    // reget for sure use real creature info selected for Pet at load/creating
    CreatureInfo const* cinfo = GetCreatureInfo();
    if (cinfo->CreatureType == CREATURE_TYPE_CRITTER)
    {
        pos.GetMap()->Add((Creature*)this);
        AIM_Initialize();
        delete result;
        return true;
    }

    m_charmInfo->SetPetNumber(pet_number, isControlled());

    SetOwnerGuid(owner->GetObjectGuid());
    SetDisplayId(fields[3].GetUInt32());
    SetNativeDisplayId(fields[3].GetUInt32());
    uint32 petlevel = fields[4].GetUInt32();
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    SetName(fields[8].GetString());

    SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_AURAS);
    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);

    if (getPetType() == HUNTER_PET)
    {
        SetByteFlag(UNIT_FIELD_BYTES_2, 2, fields[9].GetBool() ? UNIT_CAN_BE_ABANDONED : UNIT_CAN_BE_RENAMED | UNIT_CAN_BE_ABANDONED);
        SetPowerType(POWER_FOCUS);
    }
    else if (getPetType() != SUMMON_PET)
        sLog.outError("Pet have incorrect type (%u) for pet loading.", getPetType());

    if (owner->IsImmuneToNPC())
        SetImmuneToNPC(true);

    if (owner->IsImmuneToPlayer())
        SetImmuneToPlayer(true);

    if (owner->IsPvP())
        SetPvP(true);

    if (owner->IsPvPFreeForAll())
        SetPvPFreeForAll(true);

    if (owner->IsPvPSanctuary())
        SetPvPSanctuary(true);

    SetCanModifyStats(true);
    InitStatsForLevel(petlevel);
    InitTalentForLevel();                                   // set original talents points before spell loading

    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(nullptr)));
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, fields[5].GetUInt32());

    m_charmInfo->SetReactState(ReactStates(fields[6].GetUInt8()));

    uint32 savedhealth = fields[10].GetUInt32();
    uint32 savedpower = fields[11].GetUInt32();
    Powers powerType = GetPowerType();

    // load action bar, if data broken will fill later by default spells.
    m_charmInfo->LoadPetActionBar(fields[12].GetCppString());

    // since last save (in seconds)
    uint32 timediff = uint32(time(nullptr) - fields[13].GetUInt64());

    m_resetTalentsCost = fields[14].GetUInt32();
    m_resetTalentsTime = fields[15].GetUInt64();

    delete result;

    // load spells/cooldowns/auras
    _LoadAuras(timediff);

    // init AB
    LearnPetPassives();
    CastPetAuras(current);
    CastOwnerTalentAuras();
    InitTamedPetPassives(owner);
    UpdateAllStats();

    // Those two following call was moved here to fix health is not full after pet invocation (before, they where placed after map->Add())
    _LoadSpells();
    InitLevelupSpellsForLevel();
    // TODO: confirm two line above work in all situation

    // failsafe check
    savedhealth = savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth;
    savedpower = savedpower > GetMaxPower(powerType) ? GetMaxPower(powerType) : savedpower;

    if (getPetType() == SUMMON_PET)
    {
        savedhealth = GetMaxHealth();
        savedpower = GetMaxPower(powerType);
    }
    else if (!savedhealth)
    {
        if (getPetType() == HUNTER_PET && healthPercentage)
        {
            savedhealth = GetMaxHealth() * (float(healthPercentage) / 100);
            savedpower = 0;
        }
        else
            return false;
    }

    SetHealth(savedhealth);
    SetPower(powerType, savedpower);

    map->Add((Creature*)this);
    AIM_Initialize();

    CleanupActionBar();                                     // remove unknown spells from action bar after load

    _LoadSpellCooldowns();

    owner->SetPet(this);                                    // in DB stored only full controlled creature
    DEBUG_LOG("New Pet has guid %u", GetGUIDLow());

    if (owner->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)owner)->PetSpellInitialize();
        if (((Player*)owner)->GetGroup())
            ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_PET);

        ((Player*)owner)->SendTalentsInfoData(true);
    }

    if (owner->GetTypeId() == TYPEID_PLAYER && getPetType() == HUNTER_PET)
    {
        result = CharacterDatabase.PQuery("SELECT genitive, dative, accusative, instrumental, prepositional FROM character_pet_declinedname WHERE owner = '%u' AND id = '%u'", owner->GetGUIDLow(), GetCharmInfo()->GetPetNumber());

        if (result)
        {
            delete m_declinedname;
            m_declinedname = new DeclinedName;

            Field* fields2 = result->Fetch();
            for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
                m_declinedname->name[i] = fields2[i].GetCppString();

            delete result;
        }
    }

    m_loading = false;

    SynchronizeLevelWithOwner();

    SavePetToDB(PET_SAVE_AS_CURRENT);
    return true;
}

void Pet::SavePetToDB(PetSaveMode mode)
{
    if (!GetEntry())
        return;

    // save only fully controlled creature
    if (!isControlled())
        return;

    // dont save not player pets
    if (!GetOwnerGuid().IsPlayer())
        return;

    Player* pOwner = (Player*)GetOwner();
    if (!pOwner)
        return;

    // dont save shadowfiend
    if (pOwner->getClass() == CLASS_PRIEST)
        return;

    // current/stable/not_in_slot
    if (mode >= PET_SAVE_AS_CURRENT)
    {
        // reagents must be returned before save call
        if (mode == PET_SAVE_REAGENTS)
            mode = PET_SAVE_NOT_IN_SLOT;
        // not save pet as current if another pet temporary unsummoned
        else if (mode == PET_SAVE_AS_CURRENT && pOwner->GetTemporaryUnsummonedPetNumber() &&
                 pOwner->GetTemporaryUnsummonedPetNumber() != m_charmInfo->GetPetNumber())
        {
            // pet will lost anyway at restore temporary unsummoned
            if (getPetType() == HUNTER_PET)
                return;

            // for warlock case
            mode = PET_SAVE_NOT_IN_SLOT;
        }

        uint32 curhealth = GetHealth();
        uint32 curpower = GetPower(GetPowerType());

        // stable and not in slot saves
        if (mode == PET_SAVE_AS_CURRENT)
        {
            // pet is dead so it doesn't have to be shown at character login
            if (!IsAlive())
                mode = PET_SAVE_NOT_IN_SLOT;
        }
        else
        {
            if (getPetType() != HUNTER_PET)
                RemoveAllAuras();
        }

        // save pet's data as one single transaction
        CharacterDatabase.BeginTransaction();
        _SaveSpells();
        _SaveSpellCooldowns();
        _SaveAuras();

        uint32 ownerLow = GetOwnerGuid().GetCounter();
        // remove current data
        static SqlStatementID delPet ;
        static SqlStatementID insPet ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM character_pet WHERE owner = ? AND id = ?");
        stmt.PExecute(ownerLow, m_charmInfo->GetPetNumber());

        // prevent duplicate using slot (except PET_SAVE_NOT_IN_SLOT)
        if (mode <= PET_SAVE_LAST_STABLE_SLOT)
        {
            static SqlStatementID updPet ;

            stmt = CharacterDatabase.CreateStatement(updPet, "UPDATE character_pet SET slot = ? WHERE owner = ? AND slot = ?");
            stmt.PExecute(uint32(PET_SAVE_NOT_IN_SLOT), ownerLow, uint32(mode));
        }

        // prevent existence another hunter pet in PET_SAVE_AS_CURRENT and PET_SAVE_NOT_IN_SLOT
        if (getPetType() == HUNTER_PET && (mode == PET_SAVE_AS_CURRENT || mode > PET_SAVE_LAST_STABLE_SLOT))
        {
            static SqlStatementID del ;

            stmt = CharacterDatabase.CreateStatement(del, "DELETE FROM character_pet WHERE owner = ? AND (slot = ? OR slot > ?)");
            stmt.PExecute(ownerLow, uint32(PET_SAVE_AS_CURRENT), uint32(PET_SAVE_LAST_STABLE_SLOT));
        }

        // save pet
        SqlStatement savePet = CharacterDatabase.CreateStatement(insPet, "INSERT INTO character_pet ( id, entry,  owner, modelid, level, exp, Reactstate, slot, name, renamed, curhealth, "
            "curmana, abdata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType) "
             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        savePet.addUInt32(m_charmInfo->GetPetNumber());
        savePet.addUInt32(GetEntry());
        savePet.addUInt32(ownerLow);
        savePet.addUInt32(GetNativeDisplayId());
        savePet.addUInt32(GetLevel());
        savePet.addUInt32(GetUInt32Value(UNIT_FIELD_PETEXPERIENCE));
        savePet.addUInt32(uint32(m_charmInfo->GetReactState()));
        savePet.addUInt32(uint32(mode));
        savePet.addString(m_name);
        savePet.addUInt32(uint32(HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ? 0 : 1));
        savePet.addUInt32(curhealth);
        savePet.addUInt32(curpower);

        std::ostringstream ss;
        for (uint32 i = ACTION_BAR_INDEX_START; i < ACTION_BAR_INDEX_END; ++i)
        {
            ss << uint32(m_charmInfo->GetActionBarEntry(i)->GetType()) << " "
               << uint32(m_charmInfo->GetActionBarEntry(i)->GetAction()) << " ";
        };
        savePet.addString(ss);

        savePet.addUInt64(uint64(time(nullptr)));
        savePet.addUInt32(uint32(m_resetTalentsCost));
        savePet.addUInt64(uint64(m_resetTalentsTime));
        savePet.addUInt32(GetUInt32Value(UNIT_CREATED_BY_SPELL));
        savePet.addUInt32(uint32(getPetType()));

        savePet.Execute();
        CharacterDatabase.CommitTransaction();
    }
    else
    {
        RemoveAllAuras(AURA_REMOVE_BY_DELETE);
        DeleteFromDB(m_charmInfo->GetPetNumber());
    }
}

void Pet::DeleteFromDB(uint32 guidlow, bool separate_transaction)
{
    if (separate_transaction)
        CharacterDatabase.BeginTransaction();

    static SqlStatementID delPet ;
    static SqlStatementID delDeclName ;
    static SqlStatementID delAuras ;
    static SqlStatementID delSpells ;
    static SqlStatementID delSpellCD ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM character_pet WHERE id = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delDeclName, "DELETE FROM character_pet_declinedname WHERE id = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM pet_aura WHERE guid = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM pet_spell WHERE guid = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM pet_spell_cooldown WHERE guid = ?");
    stmt.PExecute(guidlow);

    if (separate_transaction)
        CharacterDatabase.CommitTransaction();
}

void Pet::DeleteFromDB(Unit* owner, PetSaveMode slot)
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT id FROM character_pet WHERE owner = '%u'  AND slot = '%u' ", owner->GetGUIDLow(), uint32(slot));
    if (result)
    {
        Field* fields = result->Fetch();
        uint32 petNumber = fields[0].GetUInt32();

        if (petNumber)
            DeleteFromDB(petNumber);

        delete result;
    }
}

void Pet::SetOwnerGuid(ObjectGuid owner)
{
    switch (uint32(m_petType))
    {
        case SUMMON_PET:
        case HUNTER_PET:
            SetSummonerGuid(owner);
            break;
    }
    Unit::SetOwnerGuid(owner);
}

void Pet::SetDeathState(DeathState s)                       // overwrite virtual Creature::SetDeathState and Unit::SetDeathState
{
    Creature::SetDeathState(s);
    if (getDeathState() == CORPSE)
    {
        // remove summoned pet (no corpse)
        if (getPetType() == SUMMON_PET)
            Unsummon(PET_SAVE_NOT_IN_SLOT);
        // other will despawn at corpse desppawning (Pet::Update code)
        else
        {
            // pet corpse non lootable and non skinnable
            SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
            RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);

        }
    }
    else if (getDeathState() == ALIVE)
    {
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
        CastPetAuras(true);
    }
    CastOwnerTalentAuras();
}

void Pet::Update(uint32 update_diff, uint32 diff)
{
    if (m_removed)                                          // pet already removed, just wait in remove queue, no updates
        return;

    switch (m_deathState)
    {
        case CORPSE:
        {
            if (m_corpseDecayTimer <= update_diff)
            {
                // pet is dead so it doesn't have to be shown at character login
                Unsummon(PET_SAVE_NOT_IN_SLOT);
                return;
            }
            break;
        }
        case ALIVE:
        {
            // unsummon pet that lost owner
            Unit* owner = GetOwner();
            if (!owner ||
                    (!IsWithinDistInMap(owner, GetMap()->GetVisibilityDistance()) && (owner->GetCharmGuid() && (owner->GetCharmGuid() != GetObjectGuid()))) ||
                    (isControlled() && !owner->GetPetGuid()))
            {
                Unsummon(PET_SAVE_REAGENTS);
                return;
            }

            if (isControlled())
            {
                if (owner->GetPetGuid() != GetObjectGuid())
                {
                    Unsummon(getPetType() == HUNTER_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, owner);
                    return;
                }
            }

            if (m_duration > 0)
            {
                if (m_duration > (int32)update_diff)
                    m_duration -= (int32)update_diff;
                else
                {
                    Unsummon(getPetType() != SUMMON_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, owner);
                    return;
                }
            }
            break;
        }
        default:
            break;
    }

    Creature::Update(update_diff, diff);
}

void Pet::RegenerateAll(uint32 update_diff)
{
    // regenerate focus for hunter pets or energy for deathknight's ghoul
    if (m_regenTimer <= update_diff)
    {
        if (!IsInCombat() || IsPolymorphed())
            RegenerateHealth();

        RegeneratePower();

        m_regenTimer = 4000;
    }
    else
        m_regenTimer -= update_diff;
}

bool Pet::CanTakeMoreActiveSpells(uint32 spellid)
{
    uint8  activecount = 1;
    uint32 chainstartstore[ACTIVE_SPELLS_MAX];

    if (IsPassiveSpell(spellid))
        return true;

    chainstartstore[0] = sSpellMgr.GetFirstSpellInChain(spellid);

    for (PetSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
            continue;

        if (IsPassiveSpell(itr->first))
            continue;

        uint32 chainstart = sSpellMgr.GetFirstSpellInChain(itr->first);

        uint8 x;

        for (x = 0; x < activecount; ++x)
        {
            if (chainstart == chainstartstore[x])
                break;
        }

        if (x == activecount)                               // spellchain not yet saved -> add active count
        {
            ++activecount;
            if (activecount > ACTIVE_SPELLS_MAX)
                return false;
            chainstartstore[x] = chainstart;
        }
    }
    return true;
}

void Pet::Unsummon(PetSaveMode mode, Unit* owner /*= nullptr*/)
{
    if (!owner)
        owner = GetOwner();

    CombatStop();

    if (owner)
    {
        if (GetOwnerGuid() != owner->GetObjectGuid())
            return;

        Player* p_owner = owner->GetTypeId() == TYPEID_PLAYER ? (Player*)owner : nullptr;

        if (p_owner)
        {
            // not save secondary permanent pet as current
            if (mode == PET_SAVE_AS_CURRENT && p_owner->GetTemporaryUnsummonedPetNumber() &&
                    p_owner->GetTemporaryUnsummonedPetNumber() != GetCharmInfo()->GetPetNumber())
                mode = PET_SAVE_NOT_IN_SLOT;

            if (mode == PET_SAVE_REAGENTS)
            {
                // returning of reagents only for players, so best done here
                uint32 spellId = GetUInt32Value(UNIT_CREATED_BY_SPELL);
                SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);

                if (spellInfo)
                {
                    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
                    {
                        if (spellInfo->Reagent[i] > 0)
                        {
                            ItemPosCountVec dest;           //for succubus, voidwalker, felhunter and felguard credit soulshard when despawn reason other than death (out of range, logout)
                            uint8 msg = p_owner->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, spellInfo->Reagent[i], spellInfo->ReagentCount[i]);
                            if (msg == EQUIP_ERR_OK)
                            {
                                Item* item = p_owner->StoreNewItem(dest, spellInfo->Reagent[i], true);
                                if (p_owner->IsInWorld())
                                    p_owner->SendNewItem(item, spellInfo->ReagentCount[i], true, false);
                            }
                        }
                    }
                }
            }

            if (isControlled())
            {
                p_owner->RemovePetActionBar();

                if (p_owner->GetGroup())
                    p_owner->SetGroupUpdateFlag(GROUP_UPDATE_PET);
            }
        }

        // only if current pet in slot
        switch (getPetType())
        {
            case MINI_PET:
                if (p_owner)
                    p_owner->SetMiniPet(nullptr);
                break;
            case PROTECTOR_PET:
            case GUARDIAN_PET:
                owner->RemoveGuardian(this);
                break;
            default:
                if (owner->GetPetGuid() == GetObjectGuid())
                    owner->SetPet(nullptr);
                break;
        }
    }

    SavePetToDB(mode);
    AddObjectToRemoveList();
    m_removed = true;
}

void Pet::GivePetXP(uint32 xp)
{
    if (getPetType() != HUNTER_PET)
        return;

    if (xp < 1)
        return;

    if (!IsAlive())
        return;

    uint32 level = GetLevel();
    uint32 maxlevel = std::min(sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL), GetOwner()->GetLevel());

    // pet not receive xp for level equal to owner level
    if (level >= maxlevel)
        return;

    xp *= sWorld.getConfig(CONFIG_FLOAT_RATE_PET_XP_KILL);

    uint32 nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
    uint32 curXP = GetUInt32Value(UNIT_FIELD_PETEXPERIENCE);
    uint32 newXP = curXP + xp;

    while (newXP >= nextLvlXP && level < maxlevel)
    {
        newXP -= nextLvlXP;
        ++level;

        GivePetLevel(level);                              // also update UNIT_FIELD_PETNEXTLEVELEXP and UNIT_FIELD_PETEXPERIENCE to level start

        nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
    }

    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, level < maxlevel ? newXP : 0);
}

void Pet::GivePetLevel(uint32 level)
{
    if (!level || level == GetLevel())
        return;

    if (getPetType() == HUNTER_PET)
    {
        SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
        SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(level));
    }

    InitStatsForLevel(level);
    InitLevelupSpellsForLevel();
    InitTalentForLevel();
}

bool Pet::CreateBaseAtCreature(Creature* creature)
{
    if (!creature)
    {
        sLog.outError("CRITICAL: NULL pointer passed into CreateBaseAtCreature()");
        return false;
    }

    CreatureCreatePos pos(creature, creature->GetOrientation());

    uint32 guid = creature->GetMap()->GenerateLocalLowGuid(HIGHGUID_PET);

    BASIC_LOG("Create pet");
    uint32 pet_number = sObjectMgr.GeneratePetNumber();
    if (!Create(guid, pos, creature->GetCreatureInfo(), pet_number))
        return false;

    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
    {
        sLog.outError("CreateBaseAtCreature() failed, creatureInfo is missing!");
        return false;
    }

    SetDisplayId(creature->GetDisplayId());
    SetNativeDisplayId(creature->GetNativeDisplayId());
    SetPowerType(POWER_FOCUS);
    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(creature->GetLevel()));
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    if (CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family))
        SetName(cFamily->Name[sWorld.GetDefaultDbcLocale()]);
    else
        SetName(creature->GetNameForLocaleIdx(sObjectMgr.GetDBCLocaleIndex()));

    SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_WARRIOR);
    SetByteValue(UNIT_FIELD_BYTES_0, 2, GENDER_NONE);
    SetByteValue(UNIT_FIELD_BYTES_0, 3, POWER_FOCUS);
    SetSheath(SHEATH_STATE_MELEE);

    SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_AURAS);
    SetByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED | UNIT_CAN_BE_ABANDONED);

    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED | UNIT_FLAG_RENAME);

    SetUInt32Value(UNIT_MOD_CAST_SPEED, creature->GetUInt32Value(UNIT_MOD_CAST_SPEED));

    return true;
}

void Pet::InitStatsForLevel(uint32 petlevel)
{
    Unit* owner = GetOwner();
    CreatureInfo const* cInfo = GetCreatureInfo();
    MANGOS_ASSERT(cInfo);

    SetLevel(petlevel);

    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0);

    int32 createResistance[MAX_SPELL_SCHOOL] = {0, 0, 0, 0, 0, 0, 0};

    if (getPetType() == HUNTER_PET)
    {
        SetMeleeDamageSchool(SpellSchools(SPELL_SCHOOL_NORMAL));
        SetAttackTime(BASE_ATTACK, BASE_ATTACK_TIME);
        SetAttackTime(OFF_ATTACK, BASE_ATTACK_TIME);
        SetAttackTime(RANGED_ATTACK, BASE_ATTACK_TIME);
    }
    else
    {
        SetMeleeDamageSchool(SpellSchools(cInfo->DamageSchool));
        SetAttackTime(BASE_ATTACK, cInfo->MeleeBaseAttackTime);
        SetAttackTime(OFF_ATTACK, cInfo->MeleeBaseAttackTime);
        SetAttackTime(RANGED_ATTACK, cInfo->RangedBaseAttackTime);

        createResistance[SPELL_SCHOOL_HOLY]   = cInfo->ResistanceHoly;
        createResistance[SPELL_SCHOOL_FIRE]   = cInfo->ResistanceFire;
        createResistance[SPELL_SCHOOL_NATURE] = cInfo->ResistanceNature;
        createResistance[SPELL_SCHOOL_FROST]  = cInfo->ResistanceFrost;
        createResistance[SPELL_SCHOOL_SHADOW] = cInfo->ResistanceShadow;
        createResistance[SPELL_SCHOOL_ARCANE] = cInfo->ResistanceArcane;
    }

    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        SetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + i), BASE_VALUE, float(createResistance[i]));

    float health, mana, armor, minDmg;

    switch (getPetType())
    {
        case HUNTER_PET:
        {
            CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    
            if (cFamily && cFamily->minScale > 0.0f)
            {
                float scale;
                if (GetLevel() >= cFamily->maxScaleLevel)
                    scale = cFamily->maxScale;
                else if (GetLevel() <= cFamily->minScaleLevel)
                    scale = cFamily->minScale;
                else
                    scale = cFamily->minScale + float(GetLevel() - cFamily->minScaleLevel) / cFamily->maxScaleLevel * (cFamily->maxScale - cFamily->minScale);

                SetObjectScale(scale);
                UpdateModelData();
            }

            // Max level
            if (petlevel < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
                SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(petlevel));
            else
            {
                SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
                SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);
            }

            // Info found in pet_levelstats
            if (PetLevelInfo const* pInfo = sObjectMgr.GetPetLevelInfo(1, petlevel))
            {
                for (int i = STAT_STRENGTH; i < MAX_STATS;++i)
                    SetCreateStat(Stats(i), float(pInfo->stats[i]));

                health = pInfo->health;
                mana = 0;
                armor = pInfo->armor;

                // First we divide attack time by standard attack time, and then multipy by level and damage mod.
                uint32 mDmg = (GetAttackTime(BASE_ATTACK) * petlevel) / 2000;

                // Set damage
                SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(mDmg - mDmg / 4));
                SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float((mDmg - mDmg / 4) * 1.5));
            }
            else
            {
                sLog.outErrorDb("HUNTER PET levelstats missing in DB! 'Weakifying' pet");

                for (int i = STAT_STRENGTH; i < MAX_STATS;++i)
                    SetCreateStat(Stats(i), 1.0f);

                health = 1;
                mana = 0;
                armor = 0;

                // Set damage
                SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, 1);
                SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, 1);
            }

            break;
        }
        case SUMMON_PET:
        {
            if (owner)
            {
                switch (owner->getClass())
                {
                    case CLASS_WARLOCK:
                    {
                        // the damage bonus used for pets is either fire or shadow damage, whatever is higher
                        uint32 fire = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FIRE);
                        uint32 shadow = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_SHADOW);
                        uint32 val = (fire > shadow) ? fire : shadow;

                        SetBonusDamage(int32(val * 0.15f));
                        // bonusAP += val * 0.57;
                        break;
                    }
                    case CLASS_MAGE:
                    {
                        // 40% damage bonus of mage's frost damage
                        float val = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FROST) * 0.4f;
                        if (val < 0)
                            val = 0;
                        SetBonusDamage(int32(val));
                        break;
                    }
                    default:
                        break;
                }
            }
            else
                sLog.outError("Pet::InitStatsForLevel> No owner for creature pet %s !", GetGuidStr().c_str());

            SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
            SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);

            // Info found in pet_levelstats
            if (PetLevelInfo const* pInfo = sObjectMgr.GetPetLevelInfo(cInfo->Entry, petlevel))
            {
                for (int i = STAT_STRENGTH; i < MAX_STATS;++i)
                    SetCreateStat(Stats(i), float(pInfo->stats[i]));

                health = pInfo->health;
                mana = pInfo->mana;
                armor = pInfo->armor;

                // Info found in ClassLevelStats
                if (CreatureClassLvlStats const* cCLS = sObjectMgr.GetCreatureClassLvlStats(petlevel, cInfo->UnitClass, cInfo->Expansion))
                {
                    minDmg = (cCLS->BaseDamage * cInfo->DamageVariance + (cCLS->BaseMeleeAttackPower / 14) * (cInfo->MeleeBaseAttackTime/1000)) * cInfo->DamageMultiplier;

                    // Apply custom damage setting (from config)
                    minDmg *= _GetDamageMod(cInfo->Rank);

                    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(minDmg));
                    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(minDmg * 1.5));
                }
                else
                {
                    sLog.outErrorDb("SUMMON_PET creature_template not finished (expansion field = -1) on creature %s! (entry: %u)", GetGuidStr().c_str(), cInfo->Entry);

                    float dMinLevel = cInfo->MinMeleeDmg / cInfo->MinLevel;
                    float dMaxLevel = cInfo->MaxMeleeDmg / cInfo->MaxLevel;
                    float mDmg = (dMaxLevel - ((dMaxLevel - dMinLevel) / 2)) * petlevel;
                    
                    // Set damage
                    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(mDmg - mDmg / 4));
                    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float((mDmg - mDmg / 4) * 1.5));
                }
            }
            else
            {
                sLog.outErrorDb("SUMMON_PET levelstats missing in DB! 'Weakifying' pet and giving it mana to make it obvious");

                for (int i = STAT_STRENGTH; i < MAX_STATS;++i)
                    SetCreateStat(Stats(i), 1.0f);

                health = 1;
                mana = 1;
                armor = 1;
                
                // Set damage
                SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, 1);
                SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, 1);
            }

            break;
        }
        case PROTECTOR_PET:
        case GUARDIAN_PET:
        {
            if (CreatureClassLvlStats const* cCLS = sObjectMgr.GetCreatureClassLvlStats(petlevel, cInfo->UnitClass, cInfo->Expansion))
            {
                health = cCLS->BaseHealth;
                mana = cCLS->BaseMana;
                armor = cCLS->BaseArmor;

                // Melee
                minDmg = (cCLS->BaseDamage * cInfo->DamageVariance + (cCLS->BaseMeleeAttackPower / 14) * (cInfo->MeleeBaseAttackTime/1000)) * cInfo->DamageMultiplier;

                // Get custom setting
                minDmg *= _GetDamageMod(cInfo->Rank);

                // If the damage value is not passed on as float it will result in damage = 1; but only for guardian type pets, though...
                SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(minDmg));
                SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(minDmg * 1.5));

                // Ranged
                minDmg = (cCLS->BaseDamage * cInfo->DamageVariance + (cCLS->BaseRangedAttackPower / 14) * (cInfo->RangedBaseAttackTime/1000)) * cInfo->DamageMultiplier;

                // Get custom setting
                minDmg *= _GetDamageMod(cInfo->Rank);

                SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, float(minDmg));
                SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, float(minDmg * 1.5));
            }
            else // TODO: Remove fallback to creature_template data when DB is ready
            {
                if (petlevel >= cInfo->MaxLevel)
                {
                    health = cInfo->MaxLevelHealth;
                    mana = cInfo->MaxLevelMana;
                }
                else if (petlevel <= cInfo->MinLevel)
                {
                    health = cInfo->MinLevelHealth;
                    mana = cInfo->MinLevelMana;
                }
                else
                {
                    float hMinLevel = cInfo->MinLevelHealth / cInfo->MinLevel;
                    float hMaxLevel = cInfo->MaxLevelHealth / cInfo->MaxLevel;
                    float mMinLevel = cInfo->MinLevelMana / cInfo->MinLevel;
                    float mMaxLevel = cInfo->MaxLevelMana / cInfo->MaxLevel;

                    health = (hMaxLevel - ((hMaxLevel - hMinLevel) / 2)) * petlevel;
                    mana = (mMaxLevel - ((mMaxLevel - mMinLevel) / 2)) * petlevel;
                }
                armor = cInfo->Armor;

                sLog.outErrorDb("Pet::InitStatsForLevel> Error trying to set stats for creature %s (entry: %u) using ClassLevelStats; not enough data to do it!", GetGuidStr().c_str(), cInfo->Entry);

                SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(cInfo->MinMeleeDmg));
                SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(cInfo->MaxMeleeDmg));

                SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, float(cInfo->MinRangedDmg));
                SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, float(cInfo->MaxRangedDmg));
            }

            break;
        }
        default:
            sLog.outError("Pet have incorrect type (%u) for level handling.", getPetType());
    }

    // Hunter's pets' should NOT use creature's original modifiers/multipliers
    if (getPetType() != HUNTER_PET)
    {
        health *= cInfo->HealthMultiplier;

        if (mana > 0)
            mana *= cInfo->PowerMultiplier;

        armor *= cInfo->ArmorMultiplier;
    }

    // Apply custom health setting (from config)
    health *= _GetHealthMod(cInfo->Rank);

    // A pet cannot not have health
    if (health < 1)
        health = 1;

    // Set base Health and Mana
    SetCreateHealth(health);
    SetCreateMana(mana);
    // Set base Armor
    SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, armor);

    // Need to update stats - calculates max health/mana etc
    UpdateAllStats();

    // Need to set Health to full
    SetHealth(GetMaxHealth());

    // Remove rage bar from pets (By setting rage = 0, and ensuring it stays that way by setting max rage = 0 as well)
    SetMaxPower(POWER_RAGE, 0);
    SetPower(POWER_RAGE, 0);

    return;
}

bool Pet::HaveInDiet(ItemPrototype const* item) const
{
    if (!item->FoodType)
        return false;

    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
        return false;

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    if (!cFamily)
        return false;

    const uint32 diet = cFamily->petFoodMask;
    const uint32 FoodMask = 1 << (item->FoodType - 1);
    return (diet & FoodMask) != 0;
}

uint32 Pet::GetCurrentFoodBenefitLevel(uint32 itemlevel)
{
    // -5 or greater food level
    if (GetLevel() <= itemlevel + 5)                        // possible to feed level 60 pet with level 55 level food for full effect
        return 35000;
    // -10..-6
    else if (GetLevel() <= itemlevel + 10)                  // pure guess, but sounds good
        return 17000;
    // -14..-11
    else if (GetLevel() <= itemlevel + 14)                  // level 55 food gets green on 70, makes sense to me
        return 8000;
    // -15 or less
    else
        return 0;                                           // food too low level
}

void Pet::_LoadSpellCooldowns()
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT spell,time FROM pet_spell_cooldown WHERE guid = '%u'", m_charmInfo->GetPetNumber());
    ByteBuffer cdData;
    uint32 cdCount = 0;

    if (result)
    {
        auto curTime = GetMap()->GetCurrentClockTime();
        do
        {
            Field* fields = result->Fetch();

            uint32 spell_id   = fields[0].GetUInt32();
            uint64 spell_time = fields[1].GetUInt64();

            SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
            if (!spellEntry)
            {
                sLog.outError("%s has unknown spell %u in `character_spell_cooldown`, skipping.", GetGuidStr().c_str(), spell_id);
                continue;
            }

            TimePoint spellExpireTime = std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::from_time_t(spell_time));
            std::chrono::milliseconds spellRecTime = std::chrono::milliseconds::zero();
            if (spellExpireTime > curTime)
                spellRecTime = std::chrono::duration_cast<std::chrono::milliseconds>(spellExpireTime - curTime);

            // skip outdated cooldown
            if (spellRecTime == std::chrono::milliseconds::zero())
                continue;

            cdData << uint32(spell_id);
            cdData << uint32(uint32(spellRecTime.count()));
            ++cdCount;

            m_cooldownMap.AddCooldown(GetMap()->GetCurrentClockTime(), spell_id, uint32(spellRecTime.count()));
#ifdef _DEBUG
            uint32 spellCDDuration = std::chrono::duration_cast<std::chrono::seconds>(spellRecTime).count();
            sLog.outDebug("Adding spell cooldown to %s, SpellID(%u), recDuration(%us).", GetGuidStr().c_str(), spell_id, spellCDDuration);
#endif
        }
        while (result->NextRow());

        delete result;

        if (cdCount && GetOwner() && GetOwner()->GetTypeId() == TYPEID_PLAYER)
        {
            WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 1 + cdData.size());
            data << GetObjectGuid();
            data << uint8(0x0);                                     // flags (0x1, 0x2)
            data.append(cdData);
            static_cast<Player*>(GetOwner())->GetSession()->SendPacket(data);
        }
    }
}

void Pet::_SaveSpellCooldowns()
{
    static SqlStatementID delSpellCD ;
    static SqlStatementID insSpellCD ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM pet_spell_cooldown WHERE guid = ?");
    stmt.PExecute(m_charmInfo->GetPetNumber());

    TimePoint currTime = GetMap()->GetCurrentClockTime();

    for (auto& cdItr : m_cooldownMap)
    {
        auto& cdData = cdItr.second;
        if (!cdData->IsPermanent())
        {
            TimePoint sTime = currTime;
            cdData->GetSpellCDExpireTime(sTime);
            uint64 spellExpireTime = uint64(Clock::to_time_t(sTime));

            stmt = CharacterDatabase.CreateStatement(insSpellCD, "INSERT INTO pet_spell_cooldown (guid,spell,time) VALUES (?, ?, ?)");
            stmt.PExecute(m_charmInfo->GetPetNumber(), cdItr.first, spellExpireTime);
        }
    }
}

void Pet::_LoadSpells()
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT spell,active FROM pet_spell WHERE guid = '%u'", m_charmInfo->GetPetNumber());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            addSpell(fields[0].GetUInt32(), ActiveStates(fields[1].GetUInt8()), PETSPELL_UNCHANGED);
        }
        while (result->NextRow());

        delete result;
    }
}

void Pet::_SaveSpells()
{
    static SqlStatementID delSpell ;
    static SqlStatementID insSpell ;

    for (PetSpellMap::iterator itr = m_spells.begin(), next = m_spells.begin(); itr != m_spells.end(); itr = next)
    {
        ++next;

        // prevent saving family passives to DB
        if (itr->second.type == PETSPELL_FAMILY)
            continue;

        switch (itr->second.state)
        {
            case PETSPELL_REMOVED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM pet_spell WHERE guid = ? and spell = ?");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first);
                m_spells.erase(itr);
            }
            continue;
            case PETSPELL_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM pet_spell WHERE guid = ? and spell = ?");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first);

                stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO pet_spell (guid,spell,active) VALUES (?, ?, ?)");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first, uint32(itr->second.active));
            }
            break;
            case PETSPELL_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO pet_spell (guid,spell,active) VALUES (?, ?, ?)");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first, uint32(itr->second.active));
            }
            break;
            case PETSPELL_UNCHANGED:
                continue;
        }

        itr->second.state = PETSPELL_UNCHANGED;
    }
}

void Pet::_LoadAuras(uint32 timediff)
{
    RemoveAllAuras();

    QueryResult* result = CharacterDatabase.PQuery("SELECT caster_guid,item_guid,spell,stackcount,remaincharges,basepoints0,basepoints1,basepoints2,periodictime0,periodictime1,periodictime2,maxduration,remaintime,effIndexMask FROM pet_aura WHERE guid = '%u'", m_charmInfo->GetPetNumber());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid casterGuid = ObjectGuid(fields[0].GetUInt64());
            uint32 item_lowguid = fields[1].GetUInt32();
            uint32 spellid = fields[2].GetUInt32();
            uint32 stackcount = fields[3].GetUInt32();
            uint32 remaincharges = fields[4].GetUInt32();
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = fields[i + 5].GetInt32();
                periodicTime[i] = fields[i + 8].GetUInt32();
            }

            int32 maxduration = fields[11].GetInt32();
            int32 remaintime = fields[12].GetInt32();
            uint32 effIndexMask = fields[13].GetUInt32();

            SpellEntry const* spellproto = sSpellTemplate.LookupEntry<SpellEntry>(spellid);
            if (!spellproto)
            {
                sLog.outError("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            // do not load single target auras (unless they were cast by the player)
            if (casterGuid != GetObjectGuid() && IsSingleTargetSpell(spellproto))
                continue;

            if (remaintime != -1)
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                    continue;

                remaintime -= timediff * IN_MILLISECONDS;
            }

            // prevent wrong values of remaincharges
            uint32 procCharges = spellproto->ProcCharges;
            if (procCharges)
            {
                if (remaincharges <= 0 || remaincharges > procCharges)
                    remaincharges = procCharges;
            }
            else
                remaincharges = 0;

            uint32 defstackamount = spellproto->StackAmount;
            if (!defstackamount)
                stackcount = 1;
            else if (defstackamount < stackcount)
                stackcount = defstackamount;
            else if (!stackcount)
                stackcount = 1;

            SpellAuraHolder* holder = CreateSpellAuraHolder(spellproto, this, nullptr);
            holder->SetLoadedState(casterGuid, ObjectGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                    continue;

                Aura* aura = CreateAura(spellproto, SpellEffectIndex(i), nullptr, holder, this);
                if (!damage[i])
                    damage[i] = aura->GetModifier()->m_amount;

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            const bool empty = holder->IsEmptyHolder();
            if (!empty)
            {
                // reset stolen single target auras
                if (casterGuid != GetObjectGuid() && holder->GetTrackedAuraType() == TRACK_AURA_TYPE_SINGLE_TARGET)
                    holder->SetTrackedAuraType(TRACK_AURA_TYPE_NOT_TRACKED);

                holder->SetState(SPELLAURAHOLDER_STATE_DB_LOAD); // Safeguard mechanism against some actions
            }

            if (!empty && AddSpellAuraHolder(holder))
            {
                holder->SetState(SPELLAURAHOLDER_STATE_READY);
                DETAIL_LOG("Added pet auras from spellid %u", spellproto->Id);
            }
            else
                delete holder;
        }
        while (result->NextRow());

        delete result;
    }
}

void Pet::_SaveAuras()
{
    static SqlStatementID delAuras ;
    static SqlStatementID insAuras ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM pet_aura WHERE guid = ?");
    stmt.PExecute(m_charmInfo->GetPetNumber());

    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();

    if (auraHolders.empty())
        return;

    stmt = CharacterDatabase.CreateStatement(insAuras, "INSERT INTO pet_aura (guid, caster_guid, item_guid, spell, stackcount, remaincharges, "
            "basepoints0, basepoints1, basepoints2, periodictime0, periodictime1, periodictime2, maxduration, remaintime, effIndexMask) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        bool save = true;
        for (int32 j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            SpellEntry const* spellInfo = holder->GetSpellProto();
            if (spellInfo->EffectApplyAuraName[j] == SPELL_AURA_MOD_STEALTH ||
                spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AREA_AURA_OWNER ||
                spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AREA_AURA_PET)
            {
                save = false;
                break;
            }
        }

        // skip all holders from spells that are passive or channeled
        // do not save single target holders (unless they were cast by the player)
        if (save && !holder->IsPassive() && !IsChanneledSpell(holder->GetSpellProto()) && (holder->GetCasterGuid() == GetObjectGuid() || holder->GetTrackedAuraType() == TRACK_AURA_TYPE_NOT_TRACKED))
        {
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                {
                    // don't save not own area auras
                    if (aur->IsAreaAura() && holder->GetCasterGuid() != GetObjectGuid())
                        continue;

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
                continue;

            stmt.addUInt32(m_charmInfo->GetPetNumber());
            stmt.addUInt64(holder->GetCasterGuid().GetRawValue());
            stmt.addUInt32(holder->GetCastItemGuid().GetCounter());
            stmt.addUInt32(holder->GetId());
            stmt.addUInt32(holder->GetStackAmount());
            stmt.addUInt8(holder->GetAuraCharges());

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                stmt.addInt32(damage[i]);

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                stmt.addUInt32(periodicTime[i]);

            stmt.addInt32(holder->GetAuraMaxDuration());
            stmt.addInt32(holder->GetAuraDuration());
            stmt.addUInt32(effIndexMask);
            stmt.Execute();
        }
    }
}

bool Pet::addSpell(uint32 spell_id, ActiveStates active /*= ACT_DECIDE*/, PetSpellState state /*= PETSPELL_NEW*/, PetSpellType type /*= PETSPELL_NORMAL*/)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
    if (!spellInfo)
    {
        // do pet spell book cleanup
        if (state == PETSPELL_UNCHANGED)                    // spell load case
        {
            sLog.outError("Pet::addSpell: nonexistent in SpellStore spell #%u request, deleting for all pets in `pet_spell`.", spell_id);
            CharacterDatabase.PExecute("DELETE FROM pet_spell WHERE spell = '%u'", spell_id);
        }
        else
            sLog.outError("Pet::addSpell: nonexistent in SpellStore spell #%u request.", spell_id);

        return false;
    }

    PetSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr != m_spells.end())
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            m_spells.erase(itr);
            state = PETSPELL_CHANGED;
        }
        else if (state == PETSPELL_UNCHANGED && itr->second.state != PETSPELL_UNCHANGED)
        {
            // can be in case spell loading but learned at some previous spell loading
            itr->second.state = PETSPELL_UNCHANGED;

            if (active == ACT_ENABLED)
                ToggleAutocast(spell_id, true);
            else if (active == ACT_DISABLED)
                ToggleAutocast(spell_id, false);

            return false;
        }
        else
            return false;
    }

    PetSpell newspell;
    newspell.state = state;
    newspell.type = type;

    if (active == ACT_DECIDE)                               // active was not used before, so we save it's autocast/passive state here
    {
        if (IsAutocastable(spellInfo))
            newspell.active = ACT_DISABLED;
        else
            newspell.active = ACT_PASSIVE;
    }
    else
        newspell.active = active;

    // talent: unlearn all other talent ranks (high and low)
    if (TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id))
    {
        if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id))
        {
            for (int i = 0; i < MAX_TALENT_RANK; ++i)
            {
                // skip learning spell and no rank spell case
                uint32 rankSpellId = talentInfo->RankID[i];
                if (!rankSpellId || rankSpellId == spell_id)
                    continue;

                // skip unknown ranks
                if (!HasSpell(rankSpellId))
                    continue;
                removeSpell(rankSpellId, false, false);
            }
        }
    }
    else if (sSpellMgr.GetSpellRank(spell_id) != 0)
    {
        for (PetSpellMap::const_iterator itr2 = m_spells.begin(); itr2 != m_spells.end(); ++itr2)
        {
            if (itr2->second.state == PETSPELL_REMOVED)
                continue;

            uint32 const oldspell_id = itr2->first;

            if (sSpellMgr.IsRankSpellDueToSpell(spellInfo, oldspell_id))
            {
                // replace by new high rank
                if (sSpellMgr.IsHighRankOfSpell(spell_id, oldspell_id))
                {
                    newspell.active = itr2->second.active;

                    if (newspell.active == ACT_ENABLED)
                        ToggleAutocast(oldspell_id, false);

                    unlearnSpell(oldspell_id, false, false);
                    break;
                }
                // ignore new lesser rank
                else if (sSpellMgr.IsHighRankOfSpell(oldspell_id, spell_id))
                    return false;
            }
        }
    }

    m_spells[spell_id] = newspell;

    if (IsPassiveSpell(spellInfo))
        CastSpell(this, spell_id, TRIGGERED_OLD_TRIGGERED);
    else
        m_charmInfo->AddSpellToActionBar(spell_id, ActiveStates(newspell.active));

    if (newspell.active == ACT_ENABLED)
        ToggleAutocast(spell_id, true);

    uint32 talentCost = GetTalentSpellCost(spell_id);
    if (talentCost)
    {
        m_usedTalentCount += talentCost;
        UpdateFreeTalentPoints(false);
    }
    return true;
}

bool Pet::learnSpell(uint32 spell_id)
{
    // prevent duplicated entires in spell book
    if (!addSpell(spell_id))
        return false;

    if (!m_loading)
    {
        Unit* owner = GetOwner();
        if (owner && owner->GetTypeId() == TYPEID_PLAYER)
        {
            WorldPacket data(SMSG_PET_LEARNED_SPELL, 4);
            data << uint32(spell_id);
            ((Player*)owner)->GetSession()->SendPacket(data);

            ((Player*)owner)->PetSpellInitialize();
        }
    }
    return true;
}

void Pet::InitLevelupSpellsForLevel()
{
    uint32 level = GetLevel();

    if (PetLevelupSpellSet const* levelupSpells = GetCreatureInfo()->Family ? sSpellMgr.GetPetLevelupSpellList(GetCreatureInfo()->Family) : nullptr)
    {
        // PetLevelupSpellSet ordered by levels, process in reversed order
        for (PetLevelupSpellSet::const_reverse_iterator itr = levelupSpells->rbegin(); itr != levelupSpells->rend(); ++itr)
        {
            // will called first if level down
            if (itr->first > level)
                unlearnSpell(itr->second, true);            // will learn prev rank if any
            // will called if level up
            else
                learnSpell(itr->second);                    // will unlearn prev rank if any
        }
    }

    int32 petSpellsId = GetCreatureInfo()->PetSpellDataId ? -(int32)GetCreatureInfo()->PetSpellDataId : GetEntry();

    // default spells (can be not learned if pet level (as owner level decrease result for example) less first possible in normal game)
    if (PetDefaultSpellsEntry const* defSpells = sSpellMgr.GetPetDefaultSpellsEntry(petSpellsId))
    {
        for (int i = 0; i < MAX_CREATURE_SPELL_DATA_SLOT; ++i)
        {
            SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(defSpells->spellid[i]);
            if (!spellEntry)
                continue;

            // will called first if level down
            if(spellEntry->SpellLevel > level)
                unlearnSpell(spellEntry->Id,true);
            // will called if level up
            else
                learnSpell(spellEntry->Id);
        }
    }
}

bool Pet::unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    if (removeSpell(spell_id, learn_prev, clear_ab))
    {
        if (!m_loading)
        {
            if (Unit* owner = GetOwner())
            {
                if (owner->GetTypeId() == TYPEID_PLAYER)
                {
                    WorldPacket data(SMSG_PET_REMOVED_SPELL, 4);
                    data << uint32(spell_id);
                    ((Player*)owner)->GetSession()->SendPacket(data);
                }
            }
        }
        return true;
    }
    return false;
}

bool Pet::removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    PetSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return false;

    if (itr->second.state == PETSPELL_REMOVED)
        return false;

    if (itr->second.state == PETSPELL_NEW)
        m_spells.erase(itr);
    else
        itr->second.state = PETSPELL_REMOVED;

    RemoveAurasDueToSpell(spell_id);

    uint32 talentCost = GetTalentSpellCost(spell_id);
    if (talentCost > 0)
    {
        if (m_usedTalentCount > talentCost)
            m_usedTalentCount -= talentCost;
        else
            m_usedTalentCount = 0;

        UpdateFreeTalentPoints(false);
    }

    if (learn_prev)
    {
        if (uint32 prev_id = sSpellMgr.GetPrevSpellInChain(spell_id))
            learnSpell(prev_id);
        else
            learn_prev = false;
    }

    // if remove last rank or non-ranked then update action bar at server and client if need
    if (clear_ab && !learn_prev && m_charmInfo->RemoveSpellFromActionBar(spell_id))
    {
        if (!m_loading)
        {
            // need update action bar for last removed rank
            if (Unit* owner = GetOwner())
                if (owner->GetTypeId() == TYPEID_PLAYER)
                    ((Player*)owner)->PetSpellInitialize();
        }
    }

    return true;
}

void Pet::CleanupActionBar()
{
    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
        if (UnitActionBarEntry const* ab = m_charmInfo->GetActionBarEntry(i))
            if (uint32 action = ab->GetAction())
                if (ab->IsActionBarForSpell() && !HasSpell(action))
                    m_charmInfo->SetActionBar(i, 0, ACT_DISABLED);
}

void Pet::InitPetCreateSpells()
{
    m_charmInfo->InitPetActionBar();
    m_spells.clear();

    LearnPetPassives();

    CastPetAuras(false);
}

bool Pet::resetTalents(bool no_cost)
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return false;

    // not need after this call
    if (((Player*)owner)->HasAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS))
        ((Player*)owner)->RemoveAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS, true);

    CreatureInfo const* ci = GetCreatureInfo();
    if (!ci)
        return false;
    // Check pet talent type
    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(ci->Family);
    if (!pet_family || pet_family->petTalentType < 0)
        return false;

    Player* player = (Player*)owner;

    if (m_usedTalentCount == 0)
    {
        UpdateFreeTalentPoints(false);                      // for fix if need counter
        return false;
    }

    uint32 cost = 0;

    if (!no_cost)
    {
        cost = resetTalentsCost();

        if (player->GetMoney() < cost)
        {
            player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
            return false;
        }
    }

    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);

        if (!talentInfo) continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);

        if (!talentTabInfo)
            continue;

        // unlearn only talents for pets family talent type
        if (!((1 << pet_family->petTalentType) & talentTabInfo->petTalentMask))
            continue;

        for (int j = 0; j < MAX_TALENT_RANK; ++j)
            if (talentInfo->RankID[j])
                removeSpell(talentInfo->RankID[j], !IsPassiveSpell(talentInfo->RankID[j]), false);
    }

    UpdateFreeTalentPoints(false);

    if (!no_cost)
    {
        player->ModifyMoney(-(int64)cost);

        m_resetTalentsCost = cost;
        m_resetTalentsTime = time(nullptr);
    }
    player->PetSpellInitialize();
    return true;
}

void Pet::resetTalentsForAllPetsOf(Player* owner, Pet* online_pet /*= nullptr*/)
{
    // not need after this call
    if (((Player*)owner)->HasAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS))
        ((Player*)owner)->RemoveAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS, true);

    // reset for online
    if (online_pet)
        online_pet->resetTalents(true);

    // now need only reset for offline pets (all pets except online case)
    uint32 except_petnumber = online_pet ? online_pet->GetCharmInfo()->GetPetNumber() : 0;

    QueryResult* resultPets = CharacterDatabase.PQuery(
                                  "SELECT id FROM character_pet WHERE owner = '%u' AND id <> '%u'",
                                  owner->GetGUIDLow(), except_petnumber);

    // no offline pets
    if (!resultPets)
        return;

    QueryResult* result = CharacterDatabase.PQuery(
                              "SELECT DISTINCT pet_spell.spell FROM pet_spell, character_pet "
                              "WHERE character_pet.owner = '%u' AND character_pet.id = pet_spell.guid AND character_pet.id <> %u",
                              owner->GetGUIDLow(), except_petnumber);

    if (!result)
    {
        delete resultPets;
        return;
    }

    bool need_comma = false;
    std::ostringstream ss;
    ss << "DELETE FROM pet_spell WHERE guid IN (";

    do
    {
        Field* fields = resultPets->Fetch();

        uint32 id = fields[0].GetUInt32();

        if (need_comma)
            ss << ",";

        ss << id;

        need_comma = true;
    }
    while (resultPets->NextRow());

    delete resultPets;

    ss << ") AND spell IN (";

    bool need_execute = false;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell = fields[0].GetUInt32();

        if (!GetTalentSpellCost(spell))
            continue;

        if (need_execute)
            ss << ",";

        ss << spell;

        need_execute = true;
    }
    while (result->NextRow());

    delete result;

    if (!need_execute)
        return;

    ss << ")";

    CharacterDatabase.Execute(ss.str().c_str());
}

void Pet::UpdateFreeTalentPoints(bool resetIfNeed)
{
    uint32 level = GetLevel();
    uint32 talentPointsForLevel = GetMaxTalentPointsForLevel(level);
    // Reset talents in case low level (on level down) or wrong points for level (hunter can unlearn TP increase talent)
    if (talentPointsForLevel == 0 || m_usedTalentCount > talentPointsForLevel)
    {
        // Remove all talent points (except for admin pets)
        if (resetIfNeed)
        {
            Unit* owner = GetOwner();
            if (!owner || owner->GetTypeId() != TYPEID_PLAYER || ((Player*)owner)->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
                resetTalents(true);
            else
                SetFreeTalentPoints(0);
        }
        else
            SetFreeTalentPoints(0);
    }
    else
        SetFreeTalentPoints(talentPointsForLevel - m_usedTalentCount);
}

void Pet::InitTalentForLevel()
{
    UpdateFreeTalentPoints();

    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    if (!m_loading)
        ((Player*)owner)->SendTalentsInfoData(true);
}

uint32 Pet::resetTalentsCost() const
{
    uint32 days = uint32(sWorld.GetGameTime() - m_resetTalentsTime) / DAY;

    // The first time reset costs 10 silver; after 1 day cost is reset to 10 silver
    if (m_resetTalentsCost < 10 * SILVER || days > 0)
        return 10 * SILVER;
    // then 50 silver
    else if (m_resetTalentsCost < 50 * SILVER)
        return 50 * SILVER;
    // then 1 gold
    else if (m_resetTalentsCost < 1 * GOLD)
        return 1 * GOLD;
    // then increasing at a rate of 1 gold; cap 10 gold
    else
        return (m_resetTalentsCost + 1 * GOLD > 10 * GOLD ? 10 * GOLD : m_resetTalentsCost + 1 * GOLD);
}

uint8 Pet::GetMaxTalentPointsForLevel(uint32 level) const
{
    uint8 points = (level >= 20) ? ((level - 16) / 4) : 0;
    // Mod points from owner SPELL_AURA_MOD_PET_TALENT_POINTS
    if (Unit* owner = GetOwner())
        points += owner->GetTotalAuraModifier(SPELL_AURA_MOD_PET_TALENT_POINTS);
    return points;
}

CharmInfo* Pet::InitCharmInfo(Unit* charm)
{
    if (!m_originalCharminfo)
    {
        m_originalCharminfo = m_charmInfo;
        m_charmInfo = new CharmInfo(charm);
    }
    return m_charmInfo;
}

void Pet::DeleteCharmInfo()
{
    if (m_originalCharminfo)
    {
        delete m_charmInfo;
        m_charmInfo = m_originalCharminfo;
        m_originalCharminfo = nullptr;
    }
}

void Pet::ToggleAutocast(uint32 spellid, bool apply)
{
    if (!IsAutocastable(spellid))
        return;

    PetSpellMap::iterator itr = m_spells.find(spellid);
    PetSpell& petSpell = itr->second;

    uint32 i;

    if (apply)
    {
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i)
            ;                                               // just search

        if (i == m_autospells.size())
        {
            m_autospells.push_back(spellid);

            if (petSpell.active != ACT_ENABLED)
            {
                petSpell.active = ACT_ENABLED;
                if (petSpell.state != PETSPELL_NEW)
                    petSpell.state = PETSPELL_CHANGED;
            }
        }
    }
    else
    {
        AutoSpellList::iterator itr2 = m_autospells.begin();
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i, ++itr2)
            ;                                               // just search

        if (i < m_autospells.size())
        {
            m_autospells.erase(itr2);
            if (petSpell.active != ACT_DISABLED)
            {
                petSpell.active = ACT_DISABLED;
                if (petSpell.state != PETSPELL_NEW)
                    petSpell.state = PETSPELL_CHANGED;
            }
        }
    }
}

bool Pet::Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, uint32 pet_number)
{
    SetMap(cPos.GetMap());
    SetPhaseMask(cPos.GetPhaseMask(), false);

    Object::_Create(guidlow, pet_number, HIGHGUID_PET);

    m_originalEntry = cinfo->Entry;

    if (!InitEntry(cinfo->Entry))
        return false;

    cPos.SelectFinalPoint(this);

    if (!cPos.Relocate(this))
        return false;

    SetSheath(SHEATH_STATE_MELEE);

    if (getPetType() == MINI_PET)                           // always non-attackable
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

    return true;
}

bool Pet::HasSpell(uint32 spell) const
{
    PetSpellMap::const_iterator itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PETSPELL_REMOVED);
}

// Get all passive spells in our skill line
void Pet::LearnPetPassives()
{
    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
        return;

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    if (!cFamily)
        return;

    PetFamilySpellsStore::const_iterator petStore = sPetFamilySpellsStore.find(cFamily->ID);
    if (petStore != sPetFamilySpellsStore.end())
    {
        for (PetFamilySpellsSet::const_iterator petSet = petStore->second.begin(); petSet != petStore->second.end(); ++petSet)
            addSpell(*petSet, ACT_DECIDE, PETSPELL_NEW, PETSPELL_FAMILY);
    }
}

void Pet::CastPetAuras(bool current)
{
    if (!isControlled())
        return;

    Unit* owner = GetOwner();

    for (PetAuraSet::const_iterator itr = owner->m_petAuras.begin(); itr != owner->m_petAuras.end();)
    {
        PetAura const* pa = *itr;
        ++itr;

        if (!current && pa->IsRemovedOnChangePet())
            owner->RemovePetAura(pa);
        else
            CastPetAura(pa);
    }
}

void Pet::CastOwnerTalentAuras()
{
    if (!GetOwner() || GetOwner()->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* pOwner = static_cast<Player*>(GetOwner());

    // Handle Ferocious Inspiration Talent
    if (pOwner && pOwner->getClass() == CLASS_HUNTER)
    {
        // clear any existing Ferocious Inspiration auras
        RemoveAurasDueToSpell(75593);
        RemoveAurasDueToSpell(75446);
        RemoveAurasDueToSpell(75447);

        if (IsAlive())
        {
            const SpellEntry* seTalent = pOwner->GetKnownTalentRankById(1800); // Ferocious Inspiration

            if (seTalent)
            {
                switch (seTalent->Id)
                {
                    case 34455: // Ferocious Inspiration Rank 1
                        CastSpell(this, 75593, TRIGGERED_OLD_TRIGGERED); // Ferocious Inspiration 1%
                        break;
                    case 34459: // Ferocious Inspiration Rank 2
                        CastSpell(this, 75446, TRIGGERED_OLD_TRIGGERED); // Ferocious Inspiration 2%
                        break;
                    case 34460: // Ferocious Inspiration Rank 3
                        CastSpell(this, 75447, TRIGGERED_OLD_TRIGGERED); // Ferocious Inspiration 3%
                        break;
                }
            }
        }
    } // End Ferocious Inspiration Talent
}

void Pet::CastPetAura(PetAura const* aura)
{
    uint32 auraId = aura->GetAura(GetEntry());
    if (!auraId)
        return;

    if (auraId == 35696)                                    // Demonic Knowledge
    {
        int32 basePoints = int32(aura->GetDamage() * (GetStat(STAT_STAMINA) + GetStat(STAT_INTELLECT)) / 100);
        CastCustomSpell(this, auraId, &basePoints, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
    }
    else
        CastSpell(this, auraId, TRIGGERED_OLD_TRIGGERED);
}

struct DoPetLearnSpell
{
    DoPetLearnSpell(Pet& _pet) : pet(_pet) {}
    void operator()(uint32 spell_id) { pet.learnSpell(spell_id); }
    Pet& pet;
};

void Pet::learnSpellHighRank(uint32 spellid)
{
    learnSpell(spellid);

    DoPetLearnSpell worker(*this);
    sSpellMgr.doForHighRanks(spellid, worker);
}

void Pet::SynchronizeLevelWithOwner()
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    switch (getPetType())
    {
        // always same level
        case SUMMON_PET:
            GivePetLevel(owner->GetLevel());
            break;
        // can't be greater owner level
        case HUNTER_PET:
            if (GetLevel() > owner->GetLevel())
                GivePetLevel(owner->GetLevel());
            else if (GetLevel() + 5 < owner->GetLevel())
                GivePetLevel(owner->GetLevel() - 5);
            break;
        default:
            break;
    }
}

void Pet::SetModeFlags(PetModeFlags mode)
{
    m_petModeFlags = mode;

    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_MODE, 12);
    data << GetObjectGuid();
    data << uint32(m_petModeFlags);
    ((Player*)owner)->GetSession()->SendPacket(data);
}

//Todo: Hack alert, remove this for a better solution when its possible
void Pet::InitTamedPetPassives(Unit* player)
{
    switch (player->getClass())
    {
        case CLASS_HUNTER:
        {
            // case 13481: Tame Beast
            player->CastSpell(this, 8875, TRIGGERED_OLD_TRIGGERED);
            break;
        }
        case CLASS_WARLOCK:
        {
            switch (GetUInt32Value(UNIT_CREATED_BY_SPELL))
            {
                case 688: // imp
                    player->CastSpell(this, 18728, TRIGGERED_OLD_TRIGGERED);
                    break;
                case 691: // felhunter
                    player->CastSpell(this, 18730, TRIGGERED_OLD_TRIGGERED);
                    break;
                case 697: // voidwalker
                    player->CastSpell(this, 18727, TRIGGERED_OLD_TRIGGERED);
                    break;
                case 712: // succubus
                    player->CastSpell(this, 18729, TRIGGERED_OLD_TRIGGERED);
                    break;
                case 30146: // felguard
                    player->CastSpell(this, 30147, TRIGGERED_OLD_TRIGGERED);
                    break;
                default:
                    break;
            }
            break;
        }
        case CLASS_SHAMAN:
        {
            player->CastSpell(this, 65221, TRIGGERED_OLD_TRIGGERED);
            break;
        }
        case CLASS_DEATH_KNIGHT:
        {
            player->CastSpell(this, 65223, TRIGGERED_OLD_TRIGGERED);
            break;
        }
        default:
            break;
    }
}
