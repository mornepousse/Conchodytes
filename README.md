<h1 align="center">Conchodytes</h1>

<p align="center">
  <em>The shrimp that lives inside another's shell.</em><br>
  <em>La crevette qui vit dans la coquille d'un autre.</em>
</p>

---

## English

A **sleeper mouse**: a replacement PCB for a Logitech M100 shell, keeping the
outer appearance of a cheap office mouse while carrying a **PMW3360** sensor and
a **wireless link to the KaSe dongle** — the same receiver as the
[Niphargus](https://github.com/mornepousse/Niphargus) keyboard.

The shell is a fixed constraint: click switches, optical wheel encoder and sensor
positions are imposed by the M100 case and are locked in the layout.

Electronics reuse the reviewed and routed blocks of Niphargus — ESP32-S3 +
nRF24L01+, TP4056 charging with DW01A/FS8205 protection, HT7833 LDO, USB-C with
ESD protection.

## Français

Une **souris sleeper** : un PCB de remplacement pour coque Logitech M100, qui
garde l'apparence d'une souris de bureau bon marché tout en embarquant un capteur
**PMW3360** et une **liaison sans fil vers le dongle KaSe** — le même récepteur
que le clavier [Niphargus](https://github.com/mornepousse/Niphargus).

La coque est une contrainte figée : les positions des clics, de l'encodeur
optique de molette et du capteur sont imposées par le boîtier M100, et
verrouillées dans le layout.

L'électronique reprend les blocs relus et routés du Niphargus — ESP32-S3 +
nRF24L01+, charge TP4056 avec protection DW01A/FS8205, LDO HT7833, USB-C protégé.

**État (2026-08)** — projet créé, blocs d'alimentation et USB transposés, feuille
MCU à élaguer, capteur à câbler. Rien n'est routé.

---

## Repository layout · Organisation

| Path | Contents |
|---|---|
| `hardware/pcb/` | KiCad project — `conchodytes.kicad_pro` |
| `scripts/` | Anti-regression checks (same pipeline as Niphargus) |
| `docs/` | Design notes |

### Fabrication

The two projects stay **independent**. They are only merged **at gerber export
time**, so the mouse board is panelised into the free space of the Niphargus
panel and costs almost nothing to make.

Les deux projets restent **indépendants**. Ils ne sont réunis qu'**au moment de
l'export gerber**, la carte souris venant se loger dans le vide du panneau
Niphargus — le PCB revient alors à presque rien.

### Anti-regression · Anti-régression

```sh
./scripts/check.sh --fast   # ERC vs committed baseline
./scripts/check.sh          # + full DRC
```

## Credits

Outline and mechanical positions derive from
[RandomDelta6/USB-Mouse](https://github.com/RandomDelta6/USB-Mouse), a working
USB mouse PCB reusing the original optical assembly.
