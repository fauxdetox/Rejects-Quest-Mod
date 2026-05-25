# RejectsQuestMod - DayZ Mod

RejectsQuestMod is a configurable quest system for DayZ. Players interact with NPCs,
accept quests, track active objectives in the HUD, complete tasks, receive rewards
and build a completed quest journal. Most gameplay content is controlled by JSON
configuration, stringtable translations and layout files.

The mod currently supports collect, kill and delivery quests, NPC quest boards,
repeatable quests, prerequisite chains, admin tools, translated UI, configurable
inputs, quest completion sounds and a field journal.

---

## Main Features

- Collect quests using item classnames.
- Kill quests using classnames, partial matches, inheritance, groups and modded target lists.
- Delivery quests where a player receives an item and must deliver it to another NPC.
- Multiple NPCs/traders with separate quest pools.
- One active quest per NPC at the same time.
- Prerequisite quests. Locked quests stay hidden until the player can actually do them.
- Repeatable quests with configurable cooldown in hours.
- Active quest HUD with dynamic text wrapping and separated quest blocks.
- Quest menu tabs:
  - Available: quests the player can accept now.
  - Now: accepted quests currently in progress or ready to turn in.
  - Done: quests already turned in.
- Completed quest journal with pages, quest title and story text.
- Item display names resolved from DayZ config where possible, so classnames can appear
  as localized item names in the player's language.
- Stringtable based UI translations.
- UTF-8 text support for PT/BR and other languages with accents.
- Configurable keybinds in the DayZ controls menu under the `QuestTrader` category.
- Quest completion sound and ready-to-turn-in sound.
- Optional quest chat messages.
- Admin panel using admins from `QuestConfig.json`.
- External Python config editor for easier quest/NPC creation.

---

## Installation

1. Copy the `RejectsQuestMod` folder into your server mod directory.
2. Add `@RejectsQuestMod` to your server `-mod=` launch parameter.
3. Keep the bundled config examples in:

```txt
RejectsQuestMod/config/QuestConfig.json
RejectsQuestMod/config/QuestItemSettings.json
RejectsQuestMod/config/QuestJournalStories.json
```

4. On first run, the mod can write/read the server profile config at:

```txt
$profile:QuestTrader/QuestConfig.json
$profile:QuestTrader/QuestItemSettings.json
$profile:QuestTrader/QuestJournalStories.json
```

If a profile config exists, it is preferred over the bundled mod config.

---

## File Structure

```txt
RejectsQuestMod/
├── config.cpp
├── inputs.xml
├── ModInfo.c
├── stringtable.csv
├── fx/
│   ├── complete.ogg
│   └── ready.ogg
├── config/
│   ├── QuestConfig.json
│   ├── QuestItemSettings.json
│   └── QuestJournalStories.json
├── gui/
│   └── layouts/
│       ├── AdminPanel.layout
│       ├── QuestHUD.layout
│       ├── QuestJournal.layout
│       ├── QuestLog.layout
│       ├── QuestMenu.layout
│       ├── QuestMsg.layout
│       └── QuestToast.layout
└── scripts/
    ├── 3_Game/QuestTrader/
    ├── 4_World/QuestTrader/
    └── 5_Mission/QuestTrader/
```

An external helper program was also added outside the mod folder:

```txt
P:/7 - QuestTraderConfigProgram/
```

Run `run_editor.bat` there to open the Python editor for `QuestConfig.json`.

---

## Text Encoding And Translations

Save `QuestConfig.json`, `QuestItemSettings.json` and `stringtable.csv` as UTF-8.
Quest texts can use accents and PT/BR characters such as `ação`, `missão`, `coração`,
`ã`, `õ`, `ç`, `à` and `ô`.

Fixed UI text should go through `stringtable.csv`.

Layout files must use DayZ stringtable syntax:

```txt
text "#QuestTrader_MENU_AVAILABLE"
text "#QuestTrader_BUTTON_ACCEPT"
```

Scripts usually use:

```c
QT_L10n.Key("BUTTON_ACCEPT")
QT_L10n.T("BUTTON_ACCEPT")
QT_L10n.ApplyText(layoutRoot, "BtnAccept", "BUTTON_ACCEPT")
QT_L10n.ResolveText(text)
```

Use `#QuestTrader_*` in layouts and `QT_L10n.*` in scripts for global UI text.
Quest-specific dynamic text such as `title`, `description`, `acceptMessage`,
`rewardMessage` and `journalStory` comes from `QuestConfig.json`. Field journal
story text can also be separated into `QuestJournalStories.json`.

---

## Configuring QuestConfig.json

Top-level structure:

```json
{
  "AdminSteamIds": [],
  "TraderNPCPositions": [],
  "Quests": [],
  "Settings": {}
}
```

### AdminSteamIds

Admins are controlled by `QuestConfig.json` only.

```json
"AdminSteamIds": [
  "76561198000000000"
]
```

Use SteamID64 when possible. The mod checks both the DayZ identity ID and the plain
Steam ID exposed by the server.

Admin functions include:

- reload QuestTrader config;
- respawn QuestTrader NPCs;
- refresh admin log;
- complete a selected player's quest;
- reset one quest by ID/title;
- reset all quest states if the quest input is blank;
- wipe a player's QuestTrader DB and history;
- copy the selected player's SteamID64;
- show online player status, UID, SteamID64, position, saved DB path, history path,
  active quest states and recent completed quests.

The online player list is updated cleanly from server connection/disconnection events
and from the server-side player identity list. It does not rely on a constant polling
loop in the admin UI.

---

## TraderNPCPositions

Example:

```json
{
  "id": "trader_001",
  "name": "Viktor",
  "position": [11567.1, 0.0, 14739.3],
  "orientation": [151.0, 0.0, 0.0],
  "model": "SurvivorM_Mirek",
  "greeting": "Hello there. I have work for those brave enough.",
  "farewell": "",
  "outfit": [
    "ChernarusSportShirt",
    "CargoPants_Black",
    "CombatBoots_Black",
    "MilitaryBeret_CDF",
    "PlateCarrierVest_Black"
  ],
  "greetings": [
    "Hello there. I have work for those brave enough.",
    "Back again? Good. I have more tasks for you."
  ]
}
```

Fields:

- `id`: unique NPC ID. Quests reference this through `traderId` and `deliveryTraderId`.
- `name`: name shown in menus and messages.
- `position`: world position `[X, Y, Z]`. Y can be `0.0`; the spawner snaps to ground.
- `orientation`: NPC orientation `[Pitch, Yaw, Roll]`.
- `model`: survivor model classname.
- `greeting`: fallback greeting.
- `farewell`: reserved/fallback farewell text.
- `outfit`: optional list of item classnames equipped on the NPC.
- `greetings`: optional list of random greetings used near the NPC.

Supported base models:

```txt
SurvivorM_Mirek
SurvivorF_Eva
SurvivorM_Denis
SurvivorF_Irena
SurvivorF_Marta
SurvivorM_Roman
```

---

## Quest Definitions

Example:

```json
{
  "id": "quest_001",
  "traderId": "trader_001",
  "title": "Medical Supplies",
  "description": "Bring medical supplies to Viktor.",
  "type": 0,
  "repeatable": true,
  "cooldownHours": 12,
  "objectives": [
    {
      "itemClassName": "BandageDressing",
      "requiredAmount": 3,
      "description": "Collect 3 bandages"
    }
  ],
  "rewards": [
    {
      "itemClassName": "TunaCan",
      "amount": 2
    }
  ],
  "rewardMessage": "You helped someone survive another night.",
  "acceptMessage": "Find three bandages and bring them back.",
  "journalStory": "The bandages reached Viktor before sundown. It was not enough to fix the world, but it was enough to keep one more person breathing through the night.",
  "prerequisiteQuestIds": []
}
```

Required fields:

- `id`
- `traderId`
- `title`
- `type`
- `objectives` for collect/kill quests
- `rewards`

Optional fields:

- `description`
- `repeatable`
- `cooldownHours`
- `acceptMessage`
- `rewardMessage`
- `journalStory`
- `prerequisiteQuestIds`
- `Targets`
- `RequiredAmount`
- `deliveryTraderId`
- `deliveryItemClass`
- `spawnItemClass`
- `spawnPosition`
- `spawnItems`
- `spawnKillTarget`
- `killTargetSpawnPosition`
- `killTargetSpawnRadius`
- `interactionObjectClassName`
- `interactionPosition`
- `interactionDistance`
- `spawnInteractionObject`

Quest types:

```txt
0 = Collect
1 = Kill
2 = Deliver
3 = Interact
```

---

## Collect Quests

Collect quests use `itemClassName`.

```json
{
  "type": 0,
  "objectives": [
    {
      "itemClassName": "Ammo_762x39",
      "requiredAmount": 60,
      "description": "Collect 60 rounds of 7.62x39mm"
    }
  ]
}
```

The HUD and quest menu check the player's inventory for collect objectives, so items
already carried by the player can be shown as ready to deliver.

---

## Kill Quests

Kill quests use the server-side death event from `EntityAI`, so QuestTrader can
detect vanilla animals, infected, custom AI, bosses, NPCs and most modded creatures
that inherit from DayZ entity classes. The matcher supports:

- exact classnames, such as `Animal_CanisLupus_Grey`;
- partial classnames, such as `CanisLupus` or `Zmb`;
- inherited base classes, such as `ZombieBase` and `AnimalBase`;
- objective-level `Targets` arrays;
- top-level `Targets` shortcut for simple kill quests;
- reusable groups from `QuestItemSettings.json`;
- wrapper/proxy entities through hierarchy-parent checks.

Legacy configs using `entityClassName` still work:

```json
{
  "type": 1,
  "objectives": [
    {
      "entityClassName": "CanisLupus",
      "requiredAmount": 3,
      "description": "Kill 3 wolves"
    }
  ]
}
```

Recommended modern format:

```json
{
  "type": 1,
  "objectives": [
    {
      "Targets": ["ZombieBase"],
      "requiredAmount": 25,
      "description": "Kill 25 infected"
    }
  ]
}
```

Simple shortcut format. If a kill quest has no `objectives`, QuestTrader creates one
objective from top-level `Targets` and `RequiredAmount`:

```json
{
  "id": "kill_zombies",
  "traderId": "trader_001",
  "title": "Clear the Road",
  "type": 1,
  "Targets": ["ZombieBase"],
  "RequiredAmount": 25,
  "objectives": [],
  "rewards": [
    { "itemClassName": "AmmoBox_762x39_20Rnd", "amount": 1 }
  ],
  "prerequisiteQuestIds": []
}
```

`ZombieBase` counts vanilla infected, special infected and modded infected that inherit
from `ZombieBase`. `AnimalBase` does the same for animals. For custom bosses or AI,
use the exact class, a stable partial classname, or add a group in
`QuestItemSettings.json`.

Optional target spawn:

```json
{
  "type": 1,
  "spawnKillTarget": true,
  "killTargetSpawnPosition": [8077.46, 0.0, 9309.57],
  "killTargetSpawnRadius": 20.0
}
```

Kill progress can be shown through notifications/chat if `notifyOnKillProgress` is enabled.
Detailed kill diagnostics can be enabled with `EnableKillDebug`.

---

## Interaction Quests

Interaction quests use `type: 3` and complete when the player interacts with the
configured object. This includes normal DayZ actions such as opening/closing,
lowering/raising, using an action on the object, or moving items into/out of an
object inventory. The QuestTrader interaction key also works as a fallback when
the player is near the target. The target can be matched by classname, position,
or both. Existing map objects are used by default; spawning the object is optional.

```json
{
  "type": 3,
  "interactionObjectClassName": "Land_Misc_Well_Pump_Blue",
  "interactionPosition": [8077.46, 0.0, 9309.57],
  "interactionDistance": 3.0,
  "spawnInteractionObject": false,
  "objectives": [],
  "rewards": [
    { "itemClassName": "Canteen", "amount": 1 }
  ]
}
```

If `spawnInteractionObject` is `true`, the server creates
`interactionObjectClassName` at `interactionPosition` when the quest is accepted and
cleans it up when the quest is cancelled or turned in.

---

## Delivery Quests

Delivery quests give an item to the player and require delivery to another NPC.

```json
{
  "type": 2,
  "traderId": "trader_001",
  "deliveryTraderId": "trader_002",
  "deliveryItemClass": "Paper",
  "objectives": [],
  "rewards": [
    {
      "itemClassName": "NailBox",
      "amount": 1
    }
  ]
}
```

Fields:

- `traderId`: NPC that offers the quest.
- `deliveryTraderId`: NPC that receives the item.
- `deliveryItemClass`: item given to the player on accept.

If a delivery quest has no manual objective, the UI creates a synthetic delivery
objective for display. Delivery items already in the player's inventory are detected
so the player can see that the quest is ready to turn in.

---

## Prerequisite Quest Chains

Use `prerequisiteQuestIds` to chain quests:

```json
"prerequisiteQuestIds": ["quest_001", "quest_002"]
```

Quests with incomplete prerequisites are hidden from the NPC board. This keeps the
Available tab clean and only shows quests the player can actually start.

The external Python editor can create dependent quests without manually typing IDs.

---

## Repeatable Quests

Repeatable quests use:

```json
"repeatable": true,
"cooldownHours": 24
```

After turn-in, the quest enters cooldown. The UI/stringtable includes repeatable and
cooldown text such as `QuestTrader_REPEATABLE_AFTER` and `QuestTrader_AVAILABLE_IN`.

Keep `cooldownHours` at `0` only if the quest should be immediately repeatable.

---

## Journal Stories

Completed quests can appear in the field journal.

Preferred separated story file:

```txt
$profile:QuestTrader/QuestJournalStories.json
```

Bundled example:

```txt
QuestTrader/config/QuestJournalStories.json
```

Format:

```json
{
  "_comment": "QuestTrader Field Journal Stories - maps quest_id to journal story text shown on completion. Edit freely - changes take effect on server restart or config reload.",
  "stories": {
    "quest_001": "Story shown in the field journal after quest_001 is completed.",
    "quest_002": "Story shown in the field journal after quest_002 is completed."
  }
}
```

On first run, if the server profile does not have `QuestJournalStories.json`, the
mod copies the bundled file from `QuestTrader/config/QuestJournalStories.json`.

The journal always checks this separated file first by quest ID. If no matching entry
exists, it falls back to the main quest config in `QuestConfig.json`.

Fallback field in `QuestConfig.json`:

```json
"journalStory": "A small story shown in the field journal after completion."
```

Full fallback order:

1. `QuestJournalStories.json` entry for the quest ID
2. `QuestConfig.json` field `journalStory`
3. `QuestConfig.json` field `rewardMessage`
4. `QuestConfig.json` field `description`
5. stored history text

For compatibility with older configs, `QuestJournalStories.json` can also be a
simple quest-id map at the root:

```json
{
  "quest_001": "Story shown in the field journal after quest_001 is completed."
}
```

The journal uses completed quest IDs from the player's saved data/history. The
optional `order` array only controls display order; completed quests that are not
listed in `order` are still added to the journal after the ordered entries. Each
page can show up to 8 quest entries using the configured journal layout slots.

Quest `quest_202` in the example config can be used as a reference for a quest with
a separate journal story.

---

## Spawned Quest Items

Single spawned item:

```json
"spawnItemClass": "Paper",
"spawnPosition": [8077.46, 0.0, 9309.57]
```

Multiple spawned items:

```json
"spawnItems": [
  {
    "itemClass": "Paper",
    "position": [8077.46, 0.0, 9309.57]
  },
  {
    "itemClass": "Rope",
    "position": [8080.0, 0.0, 9312.0]
  }
]
```

Spawned quest items are cleaned up when the quest is cancelled or completed when the
server has a tracked object for that item.

---

## Rewards

Rewards use item classnames:

```json
"rewards": [
  {
    "itemClassName": "AmmoBox_762x39_20Rnd",
    "amount": 2
  }
]
```

The client attempts to resolve item display names through DayZ config, so classnames
can appear as localized item names in menus, HUD, objectives and rewards.

Stacking/quantity behavior for rewards is controlled through `QuestItemSettings.json`.

---

## Global Settings

Current settings:

```json
"Settings": {
  "interactionDistance": 3.5,
  "showQuestMarkersOnMap": true,
  "notifyOnKillProgress": true,
  "EnableKillDebug": false,
  "debugLogging": false,
  "ActivateSucessSound": true,
  "EnableQuestChatMessages": true
}
```

Fields:

- `interactionDistance`: distance in metres for interacting with NPCs.
- `showQuestMarkersOnMap`: reserved/feature hook for map marker behavior.
- `notifyOnKillProgress`: enables kill progress notifications.
- `EnableKillDebug`: enables detailed server logs for entity death detection, target
  matching, inheritance checks, killer resolution and progress updates.
- `debugLogging`: legacy verbose/debug flag. If `EnableKillDebug` is missing from an
  older profile config, QuestTrader uses `debugLogging` as the fallback value.
- `ActivateSucessSound`: enables `fx/complete.ogg` when a quest is completed and
  turned in. The spelling is intentionally kept as `Sucess` for config compatibility.
- `EnableQuestChatMessages`: enables/disables QuestTrader chat messages. If disabled,
  quest information remains available through HUD, menu, toast and journal.

The ready-to-turn-in sound uses `fx/ready.ogg` and is tied to the ready notification
shown when a quest becomes ready to deliver.

---

## Sounds

Configured in `config.cpp`:

```txt
QuestTrader_Complete_SoundSet -> QuestTrader/fx/complete.ogg
QuestTrader_Ready_SoundSet    -> QuestTrader/fx/ready.ogg
QuestTrader_JournalOpen_SoundSet -> QuestTrader/fx/jornal_open.ogg
QuestTrader_JournalPage_SoundSet -> QuestTrader/fx/next_pages.ogg
QuestTrader_JournalClose_SoundSet -> QuestTrader/fx/book_close.ogg
```

Behavior:

- `complete.ogg` plays after successful quest completion/turn-in when
  `ActivateSucessSound` is true.
- `ready.ogg` plays when the ready-to-turn-in notification is shown to the player.
- `jornal_open.ogg` plays when the field journal opens.
- `next_pages.ogg` plays when the player moves to the next or previous journal page.
- `book_close.ogg` plays when the field journal closes.

---

## Inputs

Inputs are registered in `inputs.xml` under the DayZ controls category:

```txt
QuestTrader
```

Default bindings:

```txt
F       = open quest board / interact with nearby QuestTrader NPC
Y       = toggle active quest HUD summary
Insert  = open admin panel
'       = open field journal
Escape  = close opened QuestTrader panels
```

Players can rebind these through the DayZ controls menu. Runtime hints use the
current bound key so the UI does not show old hardcoded keys after a player changes
their controls.

QuestTrader shortcuts are blocked while chat or known admin panels are open to avoid
menus flickering while typing or using other admin tools.

---

## Quest Menu, HUD And Journal

Quest menu:

- Available tab shows quests that can be accepted now.
- Now tab shows active/accepted quests.
- Done tab shows turned-in quests.
- Long quest names and dynamic text are wrapped/scaled to fit layout areas.

HUD:

- Displays active quests.
- Uses separated HUD content blocks for active quests.
- Tracks collection and delivery item readiness from player inventory.
- Shows ready state when a quest can be turned in.

Journal:

- Opens through the configured journal key.
- Uses completed quest IDs from player data/history.
- Shows quest title and story text.
- Paginates entries and uses configured layout slots.
- Blocks player camera/movement while reading.

---

## Admin Panel

Default key: `Insert`.

Admins are read from:

```json
"AdminSteamIds": [
  "76561198000000000"
]
```

The panel can:

- list online players;
- show selected player details;
- copy SteamID64;
- reload config;
- respawn NPCs;
- refresh logs;
- complete a selected quest for the player;
- reset a specific quest by ID/title;
- reset all quest progress if the quest input is blank;
- wipe the selected player's QuestTrader DB/history.

Player status is generated from the server-side quest state and saved player DB files,
not from client-only UI guesses.

The old real-time quest editor inside the admin panel was removed because it was heavy
and unreliable for large configs. Use the external Python config editor instead.

---

## External Quest Config Program

Path:

```txt
P:/7 - QuestTraderConfigProgram/
```

Run:

```bat
run_editor.bat
```

The editor can:

- open and save `QuestConfig.json`;
- open and save `QuestJournalStories.json`;
- create backups before overwriting;
- create/edit/duplicate/delete quests;
- create dependent quests without manually typing prerequisite IDs;
- add/edit objectives and rewards;
- add/edit NPCs;
- edit admin SteamIDs and settings;
- edit separated journal stories in an organized tab;
- generate missing journal story entries from current quest fallbacks;
- validate required fields before saving;
- save JSON as UTF-8 with accents.

It uses this README as the source for tips shown beside configurable fields.

---

## QuestItemSettings.json

`QuestItemSettings.json` controls special handling that should stay outside the main
quest list. It is loaded from the server profile first:

```txt
$profile:QuestTrader/QuestItemSettings.json
```

If the profile file does not exist, QuestTrader copies or writes the bundled file:

```txt
QuestTrader/config/QuestItemSettings.json
```

Fields:

- `QuantityCountedItems`: item classnames where `GetQuantity()` is counted as units.
  Use this for tablets, rags, planks, worms and similar stack-like items.
- `StackableRewardItems`: reward classnames that should be spawned once and have their
  quantity set to the reward amount.
- `DeerKillAliases`: legacy deer aliases kept for compatibility with old quests using
  `entityClassName: "Deer"`.
- `KillTargetGroups`: named groups for kill quest matching. A quest can target the group
  name instead of repeating every classname.

Example:

```json
{
  "QuantityCountedItems": [
    "TetracyclineAntibiotics",
    "Rag",
    "WoodenPlank",
    "Worm"
  ],
  "StackableRewardItems": [
    "MoneyRuble100",
    "TetracyclineAntibiotics",
    "Rag"
  ],
  "DeerKillAliases": [
    "CervusElaphus",
    "CapreolusCapreolus"
  ],
  "KillTargetGroups": {
    "Deer": [
      "CervusElaphus",
      "CapreolusCapreolus"
    ],
    "Infected": [
      "ZombieBase",
      "Zmb"
    ],
    "Animals": [
      "AnimalBase"
    ],
    "AI": [
      "ExpansionAI",
      "eAIBase",
      "SurvivorBase"
    ],
    "Bosses": [
      "SNAFU_BossZombie",
      "DNA_Boss",
      "MyServer_CustomBoss"
    ]
  }
}
```

How kill matching works:

1. QuestTrader receives the killed `EntityAI` from `EEKilled`.
2. It resolves the player killer from the killer object, weapon/projectile hierarchy,
   hierarchy root, or the last player that damaged the entity.
3. It builds target candidates from `entityClassName`, `Targets`, `DeerKillAliases`
   and `KillTargetGroups`.
4. It checks exact classname, inheritance through `IsKindOf`/known base inheritance,
   partial classname and wrapper/proxy hierarchy parents.
5. It caches the victim signature plus target token so repeated objectives do not
   perform unnecessary inheritance work.

Practical examples:

EXAMPLE 1 - KILL ANY INFECTED

```json
{
  "id": "kill_zombies",
  "type": 1,
  "objectives": [
    {
      "Targets": [
        "ZombieBase"
      ],
      "requiredAmount": 25,
      "description": "Kill 25 infected"
    }
  ]
}
```

`ZombieBase` counts vanilla zombies, special zombies, variants and modded infected
that inherit from `ZombieBase`.

EXAMPLE 2 - HUNT ANIMALS

```json
{
  "id": "hunt_animals",
  "type": 1,
  "objectives": [
    {
      "Targets": [
        "AnimalBase"
      ],
      "requiredAmount": 10,
      "description": "Hunt 10 animals"
    }
  ]
}
```

EXAMPLE 3 - CUSTOM BOSS

```json
{
  "id": "kill_boss",
  "type": 1,
  "objectives": [
    {
      "Targets": [
        "SNAFU_BossZombie"
      ],
      "requiredAmount": 1,
      "description": "Kill the boss"
    }
  ]
}
```

EXAMPLE 4 - MULTIPLE ENTITIES

```json
{
  "id": "mixed_hunt",
  "type": 1,
  "objectives": [
    {
      "Targets": [
        "ZombieBase",
        "AnimalBase",
        "ExpansionAI"
      ],
      "requiredAmount": 50,
      "description": "Eliminate 50 hostile or wild entities"
    }
  ]
}
```

Collect quest example:

```json
{
  "id": "collect_medicine",
  "type": 0,
  "objectives": [
    {
      "itemClassName": "TetracyclineAntibiotics",
      "requiredAmount": 12,
      "description": "Collect 12 tetracycline tablets"
    }
  ]
}
```

Because `TetracyclineAntibiotics` is in `QuantityCountedItems`, QuestTrader counts the
tablet quantity instead of only counting one bottle.

Best practices:

- Use `ZombieBase` for broad infected quests.
- Use `AnimalBase` for broad hunting quests.
- Use exact boss classnames when the quest must require one specific boss.
- Use stable partial names only when the mod author uses consistent classname prefixes.
- Add server-specific categories to `KillTargetGroups` instead of duplicating long target
  lists across many quests.
- Keep `EnableKillDebug` disabled during normal play and enable it temporarily while
  testing new modded entities.

Invalid examples:

```json
{
  "type": 1,
  "objectives": [
    {
      "Targets": [],
      "requiredAmount": 10
    }
  ]
}
```

This has no target, so no kill can match.

```json
{
  "type": 1,
  "objectives": [
    {
      "Targets": [
        "ZombieBase"
      ],
      "requiredAmount": 0
    }
  ]
}
```

Use `requiredAmount` greater than zero.

Troubleshooting:

- Kill does not count: enable `EnableKillDebug`, kill one target and check the server log
  for victim classname, inheritance result, killer name and objective match result.
- Boss does not count: use the exact boss classname first. If the death event reports a
  wrapper/proxy classname, add the wrapper prefix or a group entry.
- Expansion/DNA/custom AI does not count: target the mod's shared base class if one exists,
  or add all stable AI prefixes to `KillTargetGroups`.
- Animals count inconsistently: prefer `AnimalBase` for broad hunts or concrete animal
  family names such as `CanisLupus`, `UrsusArctos` and `SusScrofa`.
- Nothing logs: confirm `EnableKillDebug` is `true` in the profile config actually used by
  the server, then reload/restart.

FAQ:

- Can I still use `entityClassName`? Yes. Existing configs remain supported.
- Should I use `Targets` or `entityClassName`? Use `Targets` for new kill quests, especially
  when one objective accepts multiple entities.
- Does `ZombieBase` include modded infected? Yes, when the modded infected properly inherits
  from `ZombieBase`. If a mod uses a separate base, add that base or prefix.
- Does partial matching still work? Yes. `Zmb` still matches vanilla infected classnames.
- Does QuestTrader scan the whole map? No. It only reacts to death events and checks active
  quests for the killer.
- What is the main limitation? A target must still produce a server-side `EntityAI` death
  event. Pure script-only systems that never call normal death hooks need a compatibility
  bridge from that mod.

---

## Finding Coordinates

Use DayZ Workbench, admin tools, or server-side debug helpers to get the player's
position. A typical script-side call is:

```c
Print(GetGame().GetPlayer().GetPosition().ToString());
```

Use X and Z from the output. Y can usually be `0.0`; the NPC spawner snaps to ground.

---

## Item And Entity Classnames

Items:

```txt
BandageDressing
Ammo_762x39
AmmoBox_762x39_20Rnd
Mosin9130
TunaCan
```

Entities:

```txt
ZombieBase
AnimalBase
CanisLupus
SNAFU_BossZombie
ZmbM_PolicemanFat
```

Kill matching supports inheritance, exact classname, partial classname, configured
groups and wrapper/proxy hierarchy parents. Use broad bases like `ZombieBase` or
`AnimalBase` for category quests, and exact boss classnames when you need strict
control.

---

## Data Storage

Player quest state is saved under:

```txt
$profile:QuestTrader/players/<uid>.json
```

Quest history/journal data is saved under:

```txt
$profile:QuestTrader/history/<uid>_history.json
```

Admin reset/wipe tools operate on these files and the in-memory state for online players.

---

## Compatibility Notes

- Designed for DayZ 1.28+.
- Uses modded `MissionServer`, `MissionGameplay` and `PlayerBase`.
- Interactions are handled through proximity and configurable input, not custom action classes.
- Layouts and translations are namespaced with `QuestTrader_*` to reduce conflicts with other mods.
- Admin logic does not depend on VPP Admin Tools or COT permissions.

---

## License

MIT - free to use, modify and redistribute with attribution.
Credit the mod author, Taco Donkey, and link to the original mod when redistributing.
