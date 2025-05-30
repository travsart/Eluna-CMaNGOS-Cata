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

#ifndef _ITEMPROTOTYPE_H
#define _ITEMPROTOTYPE_H

#include "Common.h"

enum ItemModType
{
    ITEM_MOD_NONE                     = 0,
    ITEM_MOD_HEALTH                   = 1,                  // deprecated
    ITEM_MOD_AGILITY                  = 3,
    ITEM_MOD_STRENGTH                 = 4,
    ITEM_MOD_INTELLECT                = 5,
    ITEM_MOD_SPIRIT                   = 6,
    ITEM_MOD_STAMINA                  = 7,
    ITEM_MOD_DEFENSE_SKILL_RATING     = 12,                 // deprecated
    ITEM_MOD_DODGE_RATING             = 13,
    ITEM_MOD_PARRY_RATING             = 14,
    ITEM_MOD_BLOCK_RATING             = 15,                 // deprecated
    ITEM_MOD_HIT_MELEE_RATING         = 16,                 // deprecated
    ITEM_MOD_HIT_RANGED_RATING        = 17,                 // deprecated
    ITEM_MOD_HIT_SPELL_RATING         = 18,                 // deprecated
    ITEM_MOD_CRIT_MELEE_RATING        = 19,                 // deprecated
    ITEM_MOD_CRIT_RANGED_RATING       = 20,
    ITEM_MOD_CRIT_SPELL_RATING        = 21,                 // deprecated
    ITEM_MOD_HIT_TAKEN_MELEE_RATING   = 22,                 // deprecated
    ITEM_MOD_HIT_TAKEN_RANGED_RATING  = 23,                 // deprecated
    ITEM_MOD_HIT_TAKEN_SPELL_RATING   = 24,                 // deprecated
    ITEM_MOD_CRIT_TAKEN_MELEE_RATING  = 25,                 // deprecated
    ITEM_MOD_CRIT_TAKEN_RANGED_RATING = 26,                 // deprecated
    ITEM_MOD_CRIT_TAKEN_SPELL_RATING  = 27,                 // deprecated
    ITEM_MOD_HASTE_MELEE_RATING       = 28,                 // deprecated
    ITEM_MOD_HASTE_RANGED_RATING      = 29,                 // deprecated
    ITEM_MOD_HASTE_SPELL_RATING       = 30,                 // deprecated
    ITEM_MOD_HIT_RATING               = 31,
    ITEM_MOD_CRIT_RATING              = 32,
    ITEM_MOD_HIT_TAKEN_RATING         = 33,                 // deprecated
    ITEM_MOD_CRIT_TAKEN_RATING        = 34,                 // deprecated
    ITEM_MOD_RESILIENCE_RATING        = 35,
    ITEM_MOD_HASTE_RATING             = 36,
    ITEM_MOD_EXPERTISE_RATING         = 37,
    ITEM_MOD_ATTACK_POWER             = 38,
    ITEM_MOD_RANGED_ATTACK_POWER      = 39,
    ITEM_MOD_FERAL_ATTACK_POWER       = 40,                 // deprecated
    ITEM_MOD_SPELL_HEALING_DONE       = 41,                 // deprecated
    ITEM_MOD_SPELL_DAMAGE_DONE        = 42,                 // deprecated
    ITEM_MOD_MANA_REGENERATION        = 43,                 // deprecated
    ITEM_MOD_ARMOR_PENETRATION_RATING = 44,                 // deprecated
    ITEM_MOD_SPELL_POWER              = 45,
    ITEM_MOD_HEALTH_REGEN             = 46,
    ITEM_MOD_SPELL_PENETRATION        = 47,
    ITEM_MOD_BLOCK_VALUE              = 48,                 // deprecated
    ITEM_MOD_MASTERY_RATING           = 49,
    ITEM_MOD_EXTRA_ARMOR              = 50,
    ITEM_MOD_FIRE_RESISTANCE          = 51,
    ITEM_MOD_FROST_RESISTANCE         = 52,
    ITEM_MOD_HOLY_RESISTANCE          = 53,
    ITEM_MOD_SHADOW_RESISTANCE        = 54,
    ITEM_MOD_NATURE_RESISTANCE        = 55,
    ITEM_MOD_ARCANE_RESISTANCE        = 56
};

#define MAX_ITEM_MOD                    57

enum ItemSpelltriggerType
{
    ITEM_SPELLTRIGGER_ON_USE          = 0,                  // use after equip cooldown
    ITEM_SPELLTRIGGER_ON_EQUIP        = 1,
    ITEM_SPELLTRIGGER_CHANCE_ON_HIT   = 2,
    ITEM_SPELLTRIGGER_SOULSTONE       = 4,
    ITEM_SPELLTRIGGER_ON_STORE        = 5,                  // casted at add item to inventory/equip, applied aura removed at remove item, item deleted at aura cancel/expire/etc
    ITEM_SPELLTRIGGER_LEARN_SPELL_ID  = 6                   // used in item_template.spell_2 with spell_id with SPELL_GENERIC_LEARN in spell_1
};

#define MAX_ITEM_SPELLTRIGGER           7

enum ItemBondingType
{
    NO_BIND                                     = 0,
    BIND_WHEN_PICKED_UP                         = 1,
    BIND_WHEN_EQUIPPED                          = 2,
    BIND_WHEN_USE                               = 3,
    BIND_QUEST_ITEM                             = 4,
    BIND_QUEST_ITEM1                            = 5         // not used in game
};

#define MAX_BIND_TYPE                             6

// Mask for ItemPrototype.Flags field
enum ItemPrototypeFlags
{
    ITEM_FLAG_NO_PICKUP                         = 0x00000001, // not used
    ITEM_FLAG_CONJURED                          = 0x00000002,
    ITEM_FLAG_HAS_LOOT                          = 0x00000004, // affect only non container items that can be "open" for loot. It or lockid set enable for client show "Right click to open". See also ITEM_DYNFLAG_UNLOCKED
    ITEM_FLAG_HEROIC_TOOLTIP                    = 0x00000008, // heroic item version
    ITEM_FLAG_DEPRECATED                        = 0x00000010, // can't repeat old note: appears red icon (like when item durability==0)
    ITEM_FLAG_NO_USER_DESTROY                   = 0x00000020, // used for totem. Item can not be destroyed, except by using spell (item can be reagent for spell and then allowed)
    ITEM_FLAG_PLAYERCAST                        = 0x00000040, // ? old note: usable
    ITEM_FLAG_NO_EQUIPCOOLDOWN                  = 0x00000080,
    ITEM_FLAG_MULTILOOTQUEST                    = 0x00000100, // saw this on item 47115, 49295...
    ITEM_FLAG_IS_WRAPPER                        = 0x00000200, // used or not used wrapper
    ITEM_FLAG_USES_RESOURCES                    = 0x00000400, // ignore bag space at new item creation?
    ITEM_FLAG_MULTI_DROP                        = 0x00000800, // determines if item is party loot or not
    ITEM_FLAG_ITEMPURCHASE_RECORD               = 0x00001000, // item cost can be refunded within 2 hours after purchase
    ITEM_FLAG_PETITION                          = 0x00002000, // arena/guild charter
    ITEM_FLAG_HAS_TEXT                          = 0x00004000,
    ITEM_FLAG_NO_DISENCHANT                     = 0x00008000, // a lot of items have this
    ITEM_FLAG_REAL_DURATION                     = 0x00010000, // a lot of items have this
    ITEM_FLAG_NO_CREATOR                        = 0x00020000,
    ITEM_FLAG_IS_PROSPECTABLE                   = 0x00040000, // item can have prospecting loot (in fact some items expected have empty loot)
    ITEM_FLAG_UNIQUE_EQUIPPABLE                 = 0x00080000,
    ITEM_FLAG_IGNORE_FOR_AURAS                  = 0x00100000,
    ITEM_FLAG_IGNORE_DEFAULT_ARENA_RESTRICTIONS = 0x00200000,
    ITEM_FLAG_NO_DURABILITY_LOSS                = 0x00400000, // Only items of ITEM_SUBCLASS_WEAPON_THROWN have it but not all, so can't be used as in game check
    ITEM_FLAG_USE_WHEN_SHAPESHIFTED             = 0x00800000, // last used flag in 2.3.0
    ITEM_FLAG_HAS_QUEST_GLOW                    = 0x01000000,
    ITEM_FLAG_HIDE_UNUSABLE_RECIPE              = 0x02000000,
    ITEM_FLAG_NOT_USABLE_IN_ARENA               = 0x04000000,
    ITEM_FLAG_IS_BOUND_TO_ACCOUNT               = 0x08000000, // bind on account (set in template for items that can binded in like way)
    ITEM_FLAG_NO_REAGENT_COST                   = 0x10000000, // for enchant scrolls
    ITEM_FLAG_IS_MILLABLE                       = 0x20000000, // item can have milling loot
    ITEM_FLAG_REPORT_TO_GUILD_CHAT              = 0x40000000,
    ITEM_FLAG_NO_PROGRESSIVE_LOOT               = 0x80000000, // bound item that can be traded
};

enum ItemPrototypeFlags2
{
    ITEM_FLAG2_FACTION_HORDE                    = 0x00000001, // drop in loot, sell by vendor and equipping only for horde
    ITEM_FLAG2_FACTION_ALLIANCE                 = 0x00000002, // drop in loot, sell by vendor and equipping only for alliance
    ITEM_FLAG2_DONT_IGNORE_BUY_PRICE            = 0x00000004, // item cost include gold part in case extended cost use also
    ITEM_FLAG2_CLASSIFY_AS_CASTER               = 0x00000008,
    ITEM_FLAG2_CLASSIFY_AS_PHYSICAL             = 0x00000010,
    ITEM_FLAG2_EVERYONE_CAN_ROLL_NEED           = 0x00000020,
    ITEM_FLAG2_NO_TRADE_BIND_ON_ACQUIRE         = 0x00000040,
    ITEM_FLAG2_CAN_TRADE_BIND_ON_ACQUIRE        = 0x00000080,
    ITEM_FLAG2_CAN_ONLY_ROLL_GREED              = 0x00000100, // need roll during looting is not allowed for this item
    ITEM_FLAG2_CASTER_WEAPON                    = 0x00000200, // uses caster specific dbc file for DPS calculations
    ITEM_FLAG2_DELETE_ON_LOGIN                  = 0x00000400,
    ITEM_FLAG2_INTERNAL_ITEM                    = 0x00000800,
    ITEM_FLAG2_NO_VENDOR_VALUE                  = 0x00001000,
    ITEM_FLAG2_SHOW_BEFORE_DISCOVERED           = 0x00002000,
    ITEM_FLAG2_OVERRIDE_GOLD_COST               = 0x00004000,
    ITEM_FLAG2_IGNORE_DEFAULT_RATED_BG_RESTRICTIONS = 0x00008000,
    ITEM_FLAG2_NOT_USABLE_IN_RATED_BG           = 0x00010000,
    ITEM_FLAG2_BNET_ACCOUNT_TRADE_OK            = 0x00020000,
    ITEM_FLAG2_CONFIRM_BEFORE_USE               = 0x00040000,
    ITEM_FLAG2_REEVALUATE_BONDING_ON_TRANSFORM  = 0x00080000,
    ITEM_FLAG2_NO_TRANSFORM_ON_CHARGE_DEPLETION = 0x00100000,
    ITEM_FLAG2_NO_ALTER_ITEM_VISUAL             = 0x00200000,
    ITEM_FLAG2_NO_SOURCE_FOR_ITEM_VISUAL        = 0x00400000,
    ITEM_FLAG2_IGNORE_QUALITY_FOR_ITEM_VISUAL_SOURCE = 0x00800000,
    ITEM_FLAG2_NO_DURABILITY                    = 0x01000000,
    ITEM_FLAG2_ROLE_TANK                        = 0x02000000,
    ITEM_FLAG2_ROLE_HEALER                      = 0x04000000,
    ITEM_FLAG2_ROLE_DAMAGE                      = 0x08000000,
    ITEM_FLAG2_CAN_DROP_IN_CHALLENGE_MODE       = 0x10000000,
    ITEM_FLAG2_NEVER_STACK_IN_LOOT_UI           = 0x20000000,
    ITEM_FLAG2_DISENCHANT_TO_LOOT_TABLE         = 0x40000000,
    ITEM_FLAG2_USED_IN_A_TRADESKILL             = 0x80000000
};

/*
enum ItemPrototypeFlags3
{
    ITEM_FLAG3_DONT_DESTROY_ON_QUEST_ACCEPT     = 0x00000001,
    ITEM_FLAG3_ITEM_CAN_BE_UPGRADED             = 0x00000002,
    ITEM_FLAG3_UPGRADE_FROM_ITEM_OVERRIDES_DROP_UPGRADE = 0x00000004,
    ITEM_FLAG3_ALWAYS_FFA_IN_LOOT               = 0x00000008,
    ITEM_FLAG3_HIDE_UPGRADE_LEVELS_IF_NOT_UPGRADED = 0x00000010,
    ITEM_FLAG3_UPDATE_INTERACTIONS              = 0x00000020,
    ITEM_FLAG3_UPDATE_DOESNT_LEAVE_PROGRESSIVE_WIN_HISTORY = 0x00000040,
    ITEM_FLAG3_IGNORE_ITEM_HISTORY_TRACKER      = 0x00000080,
    ITEM_FLAG3_IGNORE_ITEM_LEVEL_CAP_IN_PVP     = 0x00000100,
    ITEM_FLAG3_DISPLAY_AS_HEIRLOOM              = 0x00000200,
    ITEM_FLAG3_SKIP_USE_CHECK_ON_PICKUP         = 0x00000400,
    ITEM_FLAG3_OBSOLETE                         = 0x00000800,
    ITEM_FLAG3_DONT_DISPLAY_IN_GUILD_NEWS       = 0x00001000,
    ITEM_FLAG3_PVP_TOURNAMENT_GEAR              = 0x00002000,
    ITEM_FLAG3_REQUIRES_STACK_CHANGE_LOG        = 0x00004000,
    ITEM_FLAG3_UNUSED_FLAG                      = 0x00008000,
    ITEM_FLAG3_HIDE_NAME_SUFFIX                 = 0x00010000,
    ITEM_FLAG3_PUSH_LOOT                        = 0x00020000,
    ITEM_FLAG3_DONT_REPORT_LOOT_LOG_TO_PARTY    = 0x00040000,
    ITEM_FLAG3_ALWAYS_ALLOW_DUAL_WIELD          = 0x00080000,
    ITEM_FLAG3_OBLITERATABLE                    = 0x00100000,
    ITEM_FLAG3_ACTS_AS_TRANSMOG_HIDDEN_VISUAL_OPTION = 0x00200000,
    ITEM_FLAG3_EXPIRE_ON_WEEKLY_RESET           = 0x00400000,
    ITEM_FLAG3_DOESNT_SHOW_UP_IN_TRANSMOG_UNTIL_COLLECTED = 0x00800000,
    ITEM_FLAG3_CAN_STORE_ENCHANTS               = 0x01000000
};
*/

enum BagFamilyMask
{
    BAG_FAMILY_MASK_NONE                      = 0x00000000,
    BAG_FAMILY_MASK_ARROWS                    = 0x00000001,
    BAG_FAMILY_MASK_BULLETS                   = 0x00000002,
    BAG_FAMILY_MASK_SOUL_SHARDS               = 0x00000004,
    BAG_FAMILY_MASK_LEATHERWORKING_SUPP       = 0x00000008,
    BAG_FAMILY_MASK_INSCRIPTION_SUPP          = 0x00000010,
    BAG_FAMILY_MASK_HERBS                     = 0x00000020,
    BAG_FAMILY_MASK_ENCHANTING_SUPP           = 0x00000040,
    BAG_FAMILY_MASK_ENGINEERING_SUPP          = 0x00000080,
    BAG_FAMILY_MASK_KEYS                      = 0x00000100,
    BAG_FAMILY_MASK_GEMS                      = 0x00000200,
    BAG_FAMILY_MASK_MINING_SUPP               = 0x00000400,
    BAG_FAMILY_MASK_SOULBOUND_EQUIPMENT       = 0x00000800,
    BAG_FAMILY_MASK_VANITY_PETS               = 0x00001000,
    BAG_FAMILY_MASK_CURRENCY_TOKENS           = 0x00002000,
    BAG_FAMILY_MASK_QUEST_ITEMS               = 0x00004000,
    BAG_FAMILY_MASK_FISHING_SUPP              = 0x00008000,
};

enum SocketColor
{
    SOCKET_COLOR_META                           = 1,
    SOCKET_COLOR_RED                            = 2,
    SOCKET_COLOR_YELLOW                         = 4,
    SOCKET_COLOR_BLUE                           = 8,
    SOCKET_COLOR_HYDRAULIC                      = 16,   // unused
    SOCKET_COLOR_COGWHEEL                       = 32,
};

#define SOCKET_COLOR_ALL (SOCKET_COLOR_META | SOCKET_COLOR_RED | SOCKET_COLOR_YELLOW | SOCKET_COLOR_BLUE | SOCKET_COLOR_COGWHEEL)

enum InventoryType
{
    INVTYPE_NON_EQUIP                           = 0,
    INVTYPE_HEAD                                = 1,
    INVTYPE_NECK                                = 2,
    INVTYPE_SHOULDERS                           = 3,
    INVTYPE_BODY                                = 4,
    INVTYPE_CHEST                               = 5,
    INVTYPE_WAIST                               = 6,
    INVTYPE_LEGS                                = 7,
    INVTYPE_FEET                                = 8,
    INVTYPE_WRISTS                              = 9,
    INVTYPE_HANDS                               = 10,
    INVTYPE_FINGER                              = 11,
    INVTYPE_TRINKET                             = 12,
    INVTYPE_WEAPON                              = 13,
    INVTYPE_SHIELD                              = 14,
    INVTYPE_RANGED                              = 15,
    INVTYPE_CLOAK                               = 16,
    INVTYPE_2HWEAPON                            = 17,
    INVTYPE_BAG                                 = 18,
    INVTYPE_TABARD                              = 19,
    INVTYPE_ROBE                                = 20,
    INVTYPE_WEAPONMAINHAND                      = 21,
    INVTYPE_WEAPONOFFHAND                       = 22,
    INVTYPE_HOLDABLE                            = 23,
    INVTYPE_AMMO                                = 24,
    INVTYPE_THROWN                              = 25,
    INVTYPE_RANGEDRIGHT                         = 26,
    INVTYPE_QUIVER                              = 27,
    INVTYPE_RELIC                               = 28
};

#define MAX_INVTYPE                               29

enum ItemClass
{
    ITEM_CLASS_CONSUMABLE                       = 0,
    ITEM_CLASS_CONTAINER                        = 1,
    ITEM_CLASS_WEAPON                           = 2,
    ITEM_CLASS_GEM                              = 3,
    ITEM_CLASS_ARMOR                            = 4,
    ITEM_CLASS_REAGENT                          = 5,
    ITEM_CLASS_PROJECTILE                       = 6,
    ITEM_CLASS_TRADE_GOODS                      = 7,
    ITEM_CLASS_GENERIC                          = 8,
    ITEM_CLASS_RECIPE                           = 9,
    ITEM_CLASS_MONEY                            = 10,
    ITEM_CLASS_QUIVER                           = 11,
    ITEM_CLASS_QUEST                            = 12,
    ITEM_CLASS_KEY                              = 13,
    ITEM_CLASS_PERMANENT                        = 14,
    ITEM_CLASS_MISC                             = 15,
    ITEM_CLASS_GLYPH                            = 16
};

#define MAX_ITEM_CLASS                            17

enum ItemSubclassConsumable
{
    ITEM_SUBCLASS_CONSUMABLE                    = 0,
    ITEM_SUBCLASS_POTION                        = 1,
    ITEM_SUBCLASS_ELIXIR                        = 2,
    ITEM_SUBCLASS_FLASK                         = 3,
    ITEM_SUBCLASS_SCROLL                        = 4,
    ITEM_SUBCLASS_FOOD                          = 5,
    ITEM_SUBCLASS_ITEM_ENHANCEMENT              = 6,
    ITEM_SUBCLASS_BANDAGE                       = 7,
    ITEM_SUBCLASS_CONSUMABLE_OTHER              = 8
};

#define MAX_ITEM_SUBCLASS_CONSUMABLE              9

enum ItemSubclassContainer
{
    ITEM_SUBCLASS_CONTAINER                     = 0,
    ITEM_SUBCLASS_SOUL_CONTAINER                = 1,
    ITEM_SUBCLASS_HERB_CONTAINER                = 2,
    ITEM_SUBCLASS_ENCHANTING_CONTAINER          = 3,
    ITEM_SUBCLASS_ENGINEERING_CONTAINER         = 4,
    ITEM_SUBCLASS_GEM_CONTAINER                 = 5,
    ITEM_SUBCLASS_MINING_CONTAINER              = 6,
    ITEM_SUBCLASS_LEATHERWORKING_CONTAINER      = 7,
    ITEM_SUBCLASS_INSCRIPTION_CONTAINER         = 8,
    ITEM_SUBCLASS_FISHING_CONTAINER             = 9,
};

#define MAX_ITEM_SUBCLASS_CONTAINER               10

enum ItemSubclassWeapon
{
    ITEM_SUBCLASS_WEAPON_AXE                    = 0,
    ITEM_SUBCLASS_WEAPON_AXE2                   = 1,
    ITEM_SUBCLASS_WEAPON_BOW                    = 2,
    ITEM_SUBCLASS_WEAPON_GUN                    = 3,
    ITEM_SUBCLASS_WEAPON_MACE                   = 4,
    ITEM_SUBCLASS_WEAPON_MACE2                  = 5,
    ITEM_SUBCLASS_WEAPON_POLEARM                = 6,
    ITEM_SUBCLASS_WEAPON_SWORD                  = 7,
    ITEM_SUBCLASS_WEAPON_SWORD2                 = 8,
    ITEM_SUBCLASS_WEAPON_obsolete               = 9,
    ITEM_SUBCLASS_WEAPON_STAFF                  = 10,
    ITEM_SUBCLASS_WEAPON_EXOTIC                 = 11,
    ITEM_SUBCLASS_WEAPON_EXOTIC2                = 12,
    ITEM_SUBCLASS_WEAPON_FIST                   = 13,
    ITEM_SUBCLASS_WEAPON_MISC                   = 14,
    ITEM_SUBCLASS_WEAPON_DAGGER                 = 15,
    ITEM_SUBCLASS_WEAPON_THROWN                 = 16,
    ITEM_SUBCLASS_WEAPON_SPEAR                  = 17,
    ITEM_SUBCLASS_WEAPON_CROSSBOW               = 18,
    ITEM_SUBCLASS_WEAPON_WAND                   = 19,
    ITEM_SUBCLASS_WEAPON_FISHING_POLE           = 20
};

#define MAX_ITEM_SUBCLASS_WEAPON                  21

enum ItemSubclassGem
{
    ITEM_SUBCLASS_GEM_RED                       = 0,
    ITEM_SUBCLASS_GEM_BLUE                      = 1,
    ITEM_SUBCLASS_GEM_YELLOW                    = 2,
    ITEM_SUBCLASS_GEM_PURPLE                    = 3,
    ITEM_SUBCLASS_GEM_GREEN                     = 4,
    ITEM_SUBCLASS_GEM_ORANGE                    = 5,
    ITEM_SUBCLASS_GEM_META                      = 6,
    ITEM_SUBCLASS_GEM_SIMPLE                    = 7,
    ITEM_SUBCLASS_GEM_PRISMATIC                 = 8,
    ITEM_SUBCLASS_GEM_HYDRAULIC                 = 9,         // ABS
    ITEM_SUBCLASS_GEM_COGWHEEL                  = 10,
};

#define MAX_ITEM_SUBCLASS_GEM                     11

enum ItemSubclassArmor
{
    ITEM_SUBCLASS_ARMOR_MISC                    = 0,
    ITEM_SUBCLASS_ARMOR_CLOTH                   = 1,
    ITEM_SUBCLASS_ARMOR_LEATHER                 = 2,
    ITEM_SUBCLASS_ARMOR_MAIL                    = 3,
    ITEM_SUBCLASS_ARMOR_PLATE                   = 4,
    ITEM_SUBCLASS_ARMOR_BUCKLER                 = 5,
    ITEM_SUBCLASS_ARMOR_SHIELD                  = 6,
    ITEM_SUBCLASS_ARMOR_LIBRAM                  = 7,
    ITEM_SUBCLASS_ARMOR_IDOL                    = 8,
    ITEM_SUBCLASS_ARMOR_TOTEM                   = 9,
    ITEM_SUBCLASS_ARMOR_SIGIL                   = 10,
    ITEM_SUBCLASS_ARMOR_RELIC                   = 11,
};

#define MAX_ITEM_SUBCLASS_ARMOR                   12

enum ItemSubclassReagent
{
    ITEM_SUBCLASS_REAGENT                       = 0
};

#define MAX_ITEM_SUBCLASS_REAGENT                 1

enum ItemSubclassProjectile
{
    ITEM_SUBCLASS_WAND                          = 0,        // ABS
    ITEM_SUBCLASS_BOLT                          = 1,        // ABS
    ITEM_SUBCLASS_ARROW                         = 2,
    ITEM_SUBCLASS_BULLET                        = 3,
    ITEM_SUBCLASS_THROWN                        = 4         // ABS
};

#define MAX_ITEM_SUBCLASS_PROJECTILE              4

enum ItemSubclassTradeGoods
{
    ITEM_SUBCLASS_TRADE_GOODS                   = 0,
    ITEM_SUBCLASS_PARTS                         = 1,
    ITEM_SUBCLASS_EXPLOSIVES                    = 2,
    ITEM_SUBCLASS_DEVICES                       = 3,
    ITEM_SUBCLASS_JEWELCRAFTING                 = 4,
    ITEM_SUBCLASS_CLOTH                         = 5,
    ITEM_SUBCLASS_LEATHER                       = 6,
    ITEM_SUBCLASS_METAL_STONE                   = 7,
    ITEM_SUBCLASS_MEAT                          = 8,
    ITEM_SUBCLASS_HERB                          = 9,
    ITEM_SUBCLASS_ELEMENTAL                     = 10,
    ITEM_SUBCLASS_TRADE_GOODS_OTHER             = 11,
    ITEM_SUBCLASS_ENCHANTING                    = 12,
    ITEM_SUBCLASS_MATERIAL                      = 13,
    ITEM_SUBCLASS_VELLUM                        = 14,
};

#define MAX_ITEM_SUBCLASS_TRADE_GOODS             15

enum ItemSubclassGeneric
{
    ITEM_SUBCLASS_GENERIC                       = 0
};

#define MAX_ITEM_SUBCLASS_GENERIC                 1

enum ItemSubclassRecipe
{
    ITEM_SUBCLASS_BOOK                          = 0,
    ITEM_SUBCLASS_LEATHERWORKING_PATTERN        = 1,
    ITEM_SUBCLASS_TAILORING_PATTERN             = 2,
    ITEM_SUBCLASS_ENGINEERING_SCHEMATIC         = 3,
    ITEM_SUBCLASS_BLACKSMITHING                 = 4,
    ITEM_SUBCLASS_COOKING_RECIPE                = 5,
    ITEM_SUBCLASS_ALCHEMY_RECIPE                = 6,
    ITEM_SUBCLASS_FIRST_AID_MANUAL              = 7,
    ITEM_SUBCLASS_ENCHANTING_FORMULA            = 8,
    ITEM_SUBCLASS_FISHING_MANUAL                = 9,
    ITEM_SUBCLASS_JEWELCRAFTING_RECIPE          = 10,
    ITEM_SUBCLASS_INSCRIPTION_RECIPE            = 11,
};

#define MAX_ITEM_SUBCLASS_RECIPE                  12

enum ItemSubclassMoney
{
    ITEM_SUBCLASS_MONEY                         = 0
    //                                          = 7 one test item
};

#define MAX_ITEM_SUBCLASS_MONEY                   8

enum ItemSubclassQuiver
{
    ITEM_SUBCLASS_QUIVER0                       = 0,        // ABS
    ITEM_SUBCLASS_QUIVER1                       = 1,        // ABS
    ITEM_SUBCLASS_QUIVER                        = 2,
    ITEM_SUBCLASS_AMMO_POUCH                    = 3
};

#define MAX_ITEM_SUBCLASS_QUIVER                  4

enum ItemSubclassQuest
{
    ITEM_SUBCLASS_QUEST                         = 0
    //                                            3 one unavailable item
    //                                            8 two quest items
};

#define MAX_ITEM_SUBCLASS_QUEST                   9

enum ItemSubclassKey
{
    ITEM_SUBCLASS_KEY                           = 0,
    ITEM_SUBCLASS_LOCKPICK                      = 1
};

#define MAX_ITEM_SUBCLASS_KEY                     2

enum ItemSubclassPermanent
{
    ITEM_SUBCLASS_PERMANENT                     = 0
};

#define MAX_ITEM_SUBCLASS_PERMANENT               1

enum ItemSubclassJunk
{
    ITEM_SUBCLASS_JUNK                          = 0,
    ITEM_SUBCLASS_JUNK_REAGENT                  = 1,
    ITEM_SUBCLASS_JUNK_PET                      = 2,
    ITEM_SUBCLASS_JUNK_HOLIDAY                  = 3,
    ITEM_SUBCLASS_JUNK_OTHER                    = 4,
    ITEM_SUBCLASS_JUNK_MOUNT                    = 5
    //                                            12 single item 37677
};

#define MAX_ITEM_SUBCLASS_JUNK                    13

enum ItemSubclassGlyph
{
    ITEM_SUBCLASS_GLYPH_WARRIOR                 = 1,
    ITEM_SUBCLASS_GLYPH_PALADIN                 = 2,
    ITEM_SUBCLASS_GLYPH_HUNTER                  = 3,
    ITEM_SUBCLASS_GLYPH_ROGUE                   = 4,
    ITEM_SUBCLASS_GLYPH_PRIEST                  = 5,
    ITEM_SUBCLASS_GLYPH_DEATH_KNIGHT            = 6,
    ITEM_SUBCLASS_GLYPH_SHAMAN                  = 7,
    ITEM_SUBCLASS_GLYPH_MAGE                    = 8,
    ITEM_SUBCLASS_GLYPH_WARLOCK                 = 9,
    ITEM_SUBCLASS_GLYPH_DRUID                   = 11
};

#define MAX_ITEM_SUBCLASS_GLYPH                   12

const uint32 MaxItemSubclassValues[MAX_ITEM_CLASS] =
{
    MAX_ITEM_SUBCLASS_CONSUMABLE,
    MAX_ITEM_SUBCLASS_CONTAINER,
    MAX_ITEM_SUBCLASS_WEAPON,
    MAX_ITEM_SUBCLASS_GEM,
    MAX_ITEM_SUBCLASS_ARMOR,
    MAX_ITEM_SUBCLASS_REAGENT,
    MAX_ITEM_SUBCLASS_PROJECTILE,
    MAX_ITEM_SUBCLASS_TRADE_GOODS,
    MAX_ITEM_SUBCLASS_GENERIC,
    MAX_ITEM_SUBCLASS_RECIPE,
    MAX_ITEM_SUBCLASS_MONEY,
    MAX_ITEM_SUBCLASS_QUIVER,
    MAX_ITEM_SUBCLASS_QUEST,
    MAX_ITEM_SUBCLASS_KEY,
    MAX_ITEM_SUBCLASS_PERMANENT,
    MAX_ITEM_SUBCLASS_JUNK,
    MAX_ITEM_SUBCLASS_GLYPH
};

inline uint8 ItemSubClassToDurabilityMultiplierId(uint32 ItemClass, uint32 ItemSubClass)
{
    switch (ItemClass)
    {
        case ITEM_CLASS_WEAPON: return ItemSubClass;
        case ITEM_CLASS_ARMOR:  return ItemSubClass + 21;
    }
    return 0;
}

enum ItemExtraFlags
{
    ITEM_EXTRA_REAL_TIME_DURATION  = 0x01, // if set and have Duration time, then offline time included in counting, if not set then counted only in game time
    ITEM_EXTRA_IGNORE_QUEST_STATUS = 0x02, // if set, queststarter item will drop for player regardless of the related quest's status
    ITEM_EXTRA_ALL                 = 0x03  // all used flags, used for check DB data (mask all above flags)
};

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct _ItemStat
{
    uint32  ItemStatType;
    int32   ItemStatValue;
    uint32  ItemStatType2;
    int32   ItemStatValue2;
};
struct _Spell
{
    uint32 SpellId;                                         // id from Spell.dbc
    uint32 SpellTrigger;
    int32  SpellCharges;
    float  SpellPPMRate;
    int32  SpellCooldown;
    uint32 SpellCategory;                                   // id from SpellCategory.dbc
    int32  SpellCategoryCooldown;
};

struct _Socket
{
    uint32 Color;
    uint32 Content;
};

#define MAX_ITEM_PROTO_SOCKETS 3
#define MAX_ITEM_PROTO_SPELLS  5
#define MAX_ITEM_PROTO_STATS  10

struct ItemPrototype
{
    uint32 ItemId;
    uint32 Class;                                           // id from ItemClass.dbc
    uint32 SubClass;                                        // id from ItemSubClass.dbc
    int32  Unk0;
    char*  Name1;
    uint32 DisplayInfoID;                                   // id from ItemDisplayInfo.dbc
    uint32 Quality;
    uint32 Flags;
    uint32 Flags2;
    float Unknown;
    float Unknown1;
    uint32 Unknown2;
    uint32 BuyCount;
    uint32 BuyPrice;
    uint32 SellPrice;
    uint32 InventoryType;
    uint32 AllowableClass;
    uint32 AllowableRace;
    uint32 ItemLevel;
    uint32 RequiredLevel;
    uint32 RequiredSkill;                                   // id from SkillLine.dbc
    uint32 RequiredSkillRank;
    uint32 RequiredSpell;                                   // id from Spell.dbc
    uint32 RequiredHonorRank;
    uint32 RequiredCityRank;
    uint32 RequiredReputationFaction;                       // id from Faction.dbc
    uint32 RequiredReputationRank;
    int32  MaxCount;                                        // <=0: no limit
    int32  Stackable;                                       // 0: not allowed, -1: put in player coin info tab and don't limit stacking (so 1 slot)
    uint32 ContainerSlots;
    _ItemStat ItemStat[MAX_ITEM_PROTO_STATS];
    uint32 ScalingStatDistribution;                         // id from ScalingStatDistribution.dbc
    uint32 DamageType;
    uint32 Delay;
    float  RangedModRange;
    _Spell Spells[MAX_ITEM_PROTO_SPELLS];
    uint32 Bonding;
    char*  Description;
    uint32 PageText;
    uint32 LanguageID;
    uint32 PageMaterial;
    uint32 StartQuest;                                      // id from QuestCache.wdb
    uint32 LockID;
    int32  Material;                                        // id from Material.dbc
    uint32 Sheath;
    uint32 RandomProperty;                                  // id from ItemRandomProperties.dbc
    uint32 RandomSuffix;                                    // id from ItemRandomSuffix.dbc
    uint32 ItemSet;                                         // id from ItemSet.dbc
    uint32 MaxDurability;
    uint32 Area;                                            // id from AreaTable.dbc
    uint32 Map;                                             // id from Map.dbc
    uint32 BagFamily;                                       // bit mask (1 << id from ItemBagFamily.dbc)
    uint32 TotemCategory;                                   // id from TotemCategory.dbc
    _Socket Socket[MAX_ITEM_PROTO_SOCKETS];
    uint32 socketBonus;                                     // id from SpellItemEnchantment.dbc
    uint32 GemProperties;                                   // id from GemProperties.dbc
    int32 RequiredDisenchantSkill;
    float  ArmorDamageModifier;
    uint32 Duration;
    uint32 ItemLimitCategory;                               // id from ItemLimitCategory.dbc
    uint32 HolidayId;                                       // id from Holidays.dbc
    uint32 ScriptId;
    uint32 DisenchantID;
    uint32 FoodType;
    float  StatScalingFactor;
    uint32 Unknown400_1;
    uint32 Unknown400_2;
    uint32 MinMoneyLoot;
    uint32 MaxMoneyLoot;
    uint32 ExtraFlags;                                      // see ItemExtraFlags

    // helpers
    bool CanChangeEquipStateInCombat() const
    {
        switch (InventoryType)
        {
            case INVTYPE_RELIC:
            case INVTYPE_SHIELD:
            case INVTYPE_HOLDABLE:
                return true;
        }

        switch (Class)
        {
            case ITEM_CLASS_WEAPON:
            case ITEM_CLASS_PROJECTILE:
                return true;
        }

        return false;
    }

    uint32 GetMaxStackSize() const { return Stackable > 0 ? uint32(Stackable) : uint32(0x7FFFFFFF - 1); }
    float getDPS() const;

    uint32 GetArmor() const;
    float GetMinDamage() const { return floor(getDPS() * float(Delay) / 1000.0f * 0.7f + 0.5f); }
    float GetMaxDamage() const { return floor(getDPS() * float(Delay) / 1000.0f * 1.3f + 0.5f); }

    bool IsCurrencyToken() const { return BagFamily & BAG_FAMILY_MASK_CURRENCY_TOKENS; }

    bool IsPotion() const { return Class==ITEM_CLASS_CONSUMABLE && SubClass==ITEM_SUBCLASS_POTION; }
    bool IsConjuredConsumable() const { return Class == ITEM_CLASS_CONSUMABLE && (Flags & ITEM_FLAG_CONJURED); }
    bool IsVellum() const { return Class == ITEM_CLASS_TRADE_GOODS && (1 << SubClass) & (1 << ITEM_SUBCLASS_VELLUM); }
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

struct ItemLocale
{
    std::vector<std::string> Name;
    std::vector<std::string> Description;
};

#endif
