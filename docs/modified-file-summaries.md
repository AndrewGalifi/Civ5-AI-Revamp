# Modified Files Summary


## Strategic summary and directive selection

- `CvGameCoreDLL_Expansion2/CvGrandStrategyAI.h`
- `CvGameCoreDLL_Expansion2/CvGrandStrategyAI.cpp`

Adds the experiment-player gate, `GameStateSummary`, `StrategyDirective`, directive selection, shared tuning constants, and CSV logging.

## Experiment result logging

- `CvGameCoreDLL_Expansion2/CvGame.cpp`

Adds a small result log when the game engine declares a winner, so completed AI-only simulations can be tied back to the experiment-player setup.

## City production and city strategy

- `CvGameCoreDLL_Expansion2/CvCityAI.cpp`
- `CvGameCoreDLL_Expansion2/CvCityStrategyAI.cpp`
- `CvGameCoreDLL_Expansion2/CvCitySpecializationAI.cpp`
- `CvGameCoreDLL_Expansion2/CvWonderProductionAI.cpp`

Adds targeted production forcing or biasing for scouts, science infrastructure, workers, military response, work boats, internal trade, settlers, and wonder-building tuning.

## Research, policies, and citizens

- `CvGameCoreDLL_Expansion2/CvTechAI.cpp`
- `CvGameCoreDLL_Expansion2/CvTechClasses.cpp`
- `CvGameCoreDLL_Expansion2/CvPolicyAI.cpp`
- `CvGameCoreDLL_Expansion2/CvCityCitizens.cpp`

Adds soft research path priorities, directive-driven flavor adjustments, Tradition/Rationalism nudges, and experiment-player citizen focus overrides.

## Expansion, settlement, and worker behavior

- `CvGameCoreDLL_Expansion2/CvPlayer.cpp`
- `CvGameCoreDLL_Expansion2/CvPlayerAI.cpp`
- `CvGameCoreDLL_Expansion2/CvSiteEvaluationClasses.cpp`
- `CvGameCoreDLL_Expansion2/CvBuilderTaskingAI.cpp`
- `CvGameCoreDLL_Expansion2/CvCity.cpp`

Adds settlement guardrails, settlement suppression while going for National College, unique-luxury settlement scoring, worker tile improvement priority nudges, and adjacent unique luxury tile buying.

## Military and tactical behavior

- `CvGameCoreDLL_Expansion2/CvAIOperation.cpp`
- `CvGameCoreDLL_Expansion2/CvMilitaryAI.cpp`
- `CvGameCoreDLL_Expansion2/CvHomelandAI.cpp`
- `CvGameCoreDLL_Expansion2/CvTacticalAI.cpp`
- `CvGameCoreDLL_Expansion2/CvEconomicAI.cpp`

Adds settler escort pressure, reserve handling, explorer preservation/reassignment, and opportunistic barbarian worker recovery.

## Trade behavior

- `CvGameCoreDLL_Expansion2/CvTradeClasses.cpp`

Adds early/mid internal food trade route preference for the experiment player.
