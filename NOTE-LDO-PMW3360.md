# ⚠️ U100 — le régulateur du capteur

> **RÉSOLU SUR LA v1 le 2026-08-25.** `U100` déposé et resoudé **pins 2 et 3
> croisées en l'air**, pin 1 (VSS) laissée sur sa pastille. Rail **mesuré à
> 2,01 V**, contre 2,72 V avant — le composant monté est donc bien un
> XC6206**P202**. Le capteur fonctionne. La correction du layout reste à faire
> pour la v2 (voir « Correction retenue » plus bas).
>
> ⚠️ Le titre du fichier dit `PMW3360` : **la puce montée est un PMW3389**
> (voir `NOTES-V2.md` §0). Les plages d'alimentation sont identiques, donc tout
> ce document reste valide — seul son nom ment.

Trouvé le 2026-08-09 en cherchant une référence LCSC pour ce régulateur. **Deux
défauts**, tous deux rattrapables tant que les gerbers n'ont pas été réuploadés.

## 1. Le rail n'est pas à la bonne tension

Le rail s'appelle `+1V8`. Or la datasheet PMW3389 (*Table 4, Recommended
Operating Condition*, page 15 — mêmes valeurs que le 3360, *Table 3*, page 14)
donne :

| Paramètre | Min | **Typ** | Max |
|---|---|---|---|
| VDD | **1,80** | **1,90** | 2,10 V |
| VDDIO | 1,80 | 1,90 | 3,60 V (doit être ≥ VDD) |

**1,80 V est le minimum absolu, pas la valeur nominale.** Un LDO 1,8 V avec sa
tolérance de ±2 % descend à 1,76 V, sous la spécification. Il faut viser 1,9 ou
2,0 V. VDDIO en 3,3 V reste correct.

⚠️ Et à l'autre bout, sur le 3389, **2,10 V est un maximum ABSOLU** (*Table 3,
Absolute Maximum Ratings*, page 15 : `VDD −0,5 … 2,10 V`), pas seulement le haut
de la plage d'emploi. Le rail à 2,72 V mesuré avant correction était donc
au-dessus du maximum absolu, pas simplement hors plage recommandée.

Consommation à couvrir sur le 3389 : `IDDRUN` = **21 mA** en moyenne, LED
comprise (page 1 et page 18). Un LDO de 100 mA minimum, 120 mA de préférence —
le XC6206P202 en donne 120.

## 2. Le brochage de l'empreinte ne correspond à aucun composant

`U100` porte `lib_id = Regulator_Linear:MCP1700x-330xxTT`, mais ce symbole a son
brochage **modifié pour le HT7833 SOT-89** (1=GND, 2=VI, 3=VO) — c'est le
symbole trafiqué hérité du Niphargus, où il avait déjà causé un bloquant. Il a
été copié ici avec une empreinte **SOT-23**, boîtier où ce brochage n'existe pas.

Le PCB câble donc : pad 1 → GND, **pad 2 → +3.3V**, **pad 3 → +1V8**.

Brochage réel du XC6206 (datasheet Torex, *PIN ASSIGNMENT*, page 2) :

| | SOT-23 | SOT-89 |
|---|---|---|
| VSS | 1 | 1 |
| VIN | **3** | **2** |
| VOUT | **2** | **3** |

⚠️ **En SOT-23, les broches 2 et 3 sont inversées par rapport au câblage.** Monté
tel quel, le 3,3 V arrive sur la **sortie** du régulateur. Le pass PMOS a sa diode
de substrat orientée drain (VOUT) → source (VIN) : elle conduit, et le rail
capteur se cale à 3,3 V moins une chute de diode.

**Vérifié au banc le 2026-08-25 : 2,72 V mesuré**, pour un maximum absolu de
2,10 V. Le capteur a survécu et fonctionnait — mais c'est de la dégradation
silencieuse, pas de la marge.

Il n'existe **aucun composant qui corrigerait ça par un simple changement de
référence.** Le brochage que la carte câble — `1=GND, 2=VIN, 3=VOUT` — est la
convention **SOT-89**, jamais une convention SOT-23. Le monde du SOT-23-3
n'utilise que `1=VSS, 2=VOUT, 3=VIN` (XC6206, ME6206) ou `1=VIN, 2=GND, 3=VOUT`
(MCP1700, AP2127) — et le second mettrait VIN sur la pastille de masse. `U100`
est un brochage SOT-89 posé sur une empreinte SOT-23 : la seule issue est de
croiser les deux pistes.

Ne pas se fier aux sites de résumé de datasheets : trois d'entre eux ont donné
trois brochages différents pour ce composant, aucun correct. Seul le tableau du
fabricant fait foi.

## Correction retenue

> **APPLIQUÉE AU SCHÉMA le 2026-08-26**, et pas de la façon prévue — en mieux.
> Plutôt que d'échanger les pistes du PCB, ce sont **les numéros de pad du
> symbole** qui ont été corrigés : `VI` porte désormais le pad **3** et `VO` le
> pad **2**, soit le brochage réel du XC6206 en SOT-23.
>
> Le schéma cesse ainsi de mentir — `VI` reste dessiné à gauche avec le 3,3 V,
> `VO` à droite avec le rail capteur — et la netlist devient juste d'elle-même :
> `U100.1 → GND`, `U100.2 → +2V0`, `U100.3 → +3.3V`.
>
> **PCB à mettre à jour** : c'est le F8 qui reportera l'échange sur les pistes.

**Garder le SOT-23 et échanger les pistes des pads 2 et 3 de `U100`**, puis
commander le **XC6206P202MR-G — LCSC `C2891260`** (2,0 V, SOT-23, 120 mA,
±2 % → 1,96-2,04 V, bien dans la plage 1,80-2,10).

C'est **gratuit en v2** : deux pistes à reprendre dans le layout, sur une carte
qu'il faut de toute façon rouvrir pour D100, R101 et la rotation de U2. Le
composant ne change pas.

*(Le **XC6206P192** — 1,9 V — serait encore plus juste : c'est exactement la
valeur typique de la Table 4 et celle du schéma de référence. Le P202 reste
parfaitement bon, il est simplement un cran au-dessus du centre de cible.)*

Sur la v1 déjà fabriquée, le croisement se fait **sans couper de piste** : ne
souder que la pin 1 sur sa pastille, relever les pins 2 et 3, et poser deux fils
fins croisés. Ne pas tenter de retourner le boîtier — en SOT-23 les pins 1 et 2
sont du même côté et la 3 en face, retourner échange 1 et 2, ce qui mettrait
**VOUT sur la masse**.

L'alternative — passer l'empreinte en **SOT-89**, où le brochage 1=VSS 2=VIN
3=VOUT correspond déjà au câblage — a été écartée : le voisin le plus proche
(`R102`) est à 3,86 mm du centre de `U100`, et un SOT-89 fait 4,6 × 4,3 mm pads
compris. Ça passerait tout juste, sur une carte dont la mécanique est figée par
la coque M100.

Penser à renommer le rail `+1V8` en `+2V0` pour que le schéma ne mente pas.

## Point tranché : VDDPIX (résolu le 2026-08-25)

La broche 3 du capteur est décrite comme « **VDDPIX — Power — LDO output** »
(*Table 1*, page 3 du 3360 ; nommée `VDCPIX`, « *LDO output for selective analog
circuit* », *Table 1*, page 5 du 3389) : c'est une **sortie** de régulateur
interne. Or le PCB la
relie au rail externe, en parallèle de VDD (pad 3 et pad 4 tous deux sur le même
net). La question était de savoir si PixArt le prescrit.

**Oui.** La figure a été lue en la rendant en image plutôt qu'en tentant d'en
extraire le texte. **Figure 10, page 12** (*Reference Schematics*, schéma
d'application filaire), bloc *Sensor Block* : les broches **3 (VDDPIX)** et
**4 (VDD)** sont toutes deux reliées au même rail `VDD6_SS`, tandis que la
broche **5 (VDDIO)** part sur `VDD4_3.3V`.

Le câblage du PCB est donc conforme au schéma d'application. **Rien à corriger
sur ce point.**

*(Au passage, la même figure a livré le montage de la LED — voir le point 6 de
`NOTES-V2.md`, où D100 se révèle être un composant de trop.)*
