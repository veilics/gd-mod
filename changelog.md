## v2.22.2 - New icon
- The mod's icon is now the Glimmer gem.

## v2.22.1 - Missing textures fix
- FIXED icons, the roulette button, the background, the Glimmer gem and the win/coin sounds not loading in builds made from GitHub (Windows, macOS, the all-platforms file). The resource list pointed at the wrong folder, so those builds shipped without any of the mod's images or sounds.

## v2.22.0 - Rooms close properly + whole-roulette room bets
- FIXED rooms staying open after closing or restarting the game. Your game now checks in with the bot while you're in a room; if it stops (game closed, crashed, restarted) you're removed after 60 seconds, the host role moves to the next player, and an empty room closes. Starting the game also leaves any room you were still in.
- ROOM BETS pay out at the end of the WHOLE roulette, not after a round. When the game ends you get the same animations as a normal bet: the gem fountain and green count-up when you win, the red count-down when you lose. Globed shows WIN / LOSS under your icon, and a notification tells you the result.
- Per-attempt bets are turned off on a room's round levels so they can't resolve after round 1 - your room bet covers the game.
- Description rewritten around the mod's features.
- BOT (upload gd.js + restart).

## v2.21.1 - UI fixes + challenge rebalance
- FIXED the dark stripe through rows, badges and the in-level room scoreboard: square02b panels shorter than their own corners drew a negative middle slice. Every panel now goes through one helper that keeps the corners intact at any size.
- FIXED overlapping buttons: Solo list tabs and rarity chips are laid out from their real rendered widths; Leave / History sit right after the room name; challenge Play and reroll no longer touch.
- Challenges: glimmer gem next to every reward; text no longer runs under the reward.
- CHALLENGE REBALANCE (bot): daily rewards 300k - 10M, weekly 800k - 10B. Weeklies are mostly Insanes with exactly ONE demon (beat an Easy/Medium Demon, or a % on a Hard/Insane Demon). Daily: beat a featured+ Harder/Insane, a % on an Insane, a % on a demon. The current day/week regenerate with the new rules (completed slots stay completed).
- BOT (upload gd.js + restart).

## v2.21.0 - Team & Elimination rooms, room history, daily/weekly challenges, music fix
- TEAM ROOMS: Mode "Teams (2/3/4)". Players are auto-balanced into Red / Blue / Green / Yellow (switch with the Team button in the lobby). A round win scores for your team; the winning team splits the pot (each also gets their own bet back). Team colours on the lobby list and the in-level scoreboard.
- ELIMINATION MODE: rounds have a clock (1-10 min). Clear the target to survive the round; if everyone but one clears, that one is out immediately; if everyone clears, the slowest is out; when the clock runs out, the lowest % among those who did not clear is out. Progressive rooms raise the target +10% each round. Last one standing takes the pot. Eliminated players spectate (OUT on the lists).
- ROOM HISTORY: History button in the lobby - every round of the game (level, winner and %, or who was eliminated) and the room's past games.
- Live room feed: round wins, eliminations, joins, team changes and game results come in as notifications for everyone.
- AFTER THE LAST ROUND you are taken out of the level, off the level page and back to the roulette screen with the room open. Round levels now REPLACE the previous level page instead of stacking, so Back never walks through old levels.
- GLOBED: while you are in a roulette room, everyone in the level who is not in your room is hidden (player + progress icon) - like being in a private Globed room.
- DAILY / WEEKLY CHALLENGES (Roulette home -> Challenges): per-player tasks like "Beat a Mythic Harder / Insane", "Get 45% on a demon", "Beat a Legendary Easy/Medium/Hard Demon", "Get 20% on an Extreme Demon". Your game picks the exact level from RobTop and CHECKS your save: levels you already beat (or are already at the target % on) are skipped. Claimed from real, non-practice runs; one free reroll per challenge; shows your best % and the reset timer.
- MUSIC FIXED: levels showed Stereo Madness until opened once. Level objects are now built from RobTop's raw fields directly - custom song (35), official track (12), 2.2 song/SFX lists (52/53) - and GD is asked for the song info right away.
- Solo: the extra difficulty names are gone (the faces already say them), icons bigger, boxes always behind the icons. The roulette options button (bottom-right) is removed - everything is on the Solo screen.
- BOT (upload gd.js + restart): team/elimination engine with round clock, events + history + past games, rooms/team, challenges / challenges/assign / reroll / claim.

## v2.20.0 - Roulette home screen, new Solo setup, songs fixed
- NEW HOME: the roulette opens on two big square cards - SOLO (classic roulette; says "Resume your run" when one is paused) and MULTIPLAYER (rooms; shows your room / round or how many rooms are open). Your Glimmer balance sits under them. The back arrow goes Solo -> Home -> out.
- NEW SOLO SETUP: list tabs (Levels / Demonlist / Challenge / GD List), a full difficulty picker with the real GD faces - Easy, Normal, Hard, Harder, Insane on top, then all six demon tiers (Any, Easy, Medium, Hard, Insane, Extreme) directly, no extra popup - with the pick highlighted; rarity chips (Any / Featured / Epic / Legendary / Mythic); skips stepper; Start. GD List shows a "Set List ID" button. The old difficulty row, plus button and frame title are retired.
- SONGS FIXED: roulette and room levels were rebuilt by hand from a few fields (room rounds from only id + name), so GD never got the song data (2.2 song/SFX lists, the response's song info). Every RobTop response now goes through GD's own parser (GameLevelManager::responseToDict + GJGameLevel::create, MusicDownloadManager::createSongsInfo), exactly like a level search.
- GRAY GOAL % IS BACK: the "(goal%)" next to the percentage only appeared if the level was already downloaded when its page opened. It now attaches live on the level page (also after the download finishes), in-level next to the % and on the pause screen - for the solo roulette goal and multiplayer round targets.
- ROOM SCOREBOARD only shows on the room's round level (no more "Not the round level").
- Bot unchanged - no PebbleHost upload needed.

## v2.19.1 - Levels fetched the GD-Roulette way (no more GDBrowser 403)
- SOLO ROULETTE: the level fetcher had been rewritten to go through the bot, which fell back to GDBrowser (now 403). It is back to the original GD-Roulette code path, ported to Geode v5: the game POSTs straight to RobTop's getGJLevels21.php (type=0 search, star=1, len=0..4, diff / demonFilter, empty User-Agent, public secret), learns the page count for the query, then picks a random page. Demonlist = Pointercrate main list -> RobTop lookup by id; Challenge List and GD List (by id) restored exactly like upstream. The rarity filter uses RobTop's own featured/epic/legendary/mythic search flags.
- MULTIPLAYER ROOMS: each player's game keeps one RobTop-rolled level ready (same request as above) and sends it with Start / Ready / Skip / progress, so rounds still get a level even when RobTop or GDBrowser blocks the bot's host.
- BOT (upload gd.js + restart): level picking rewritten the same way (type=0 search with the real page count instead of 'most downloaded' on a blind page 0-80 that kept falling through to GDBrowser); trending = RobTop's trending search; level-by-id and daily/weekly via RobTop. No GDBrowser in any roulette path. Rooms accept the game-rolled level.

## v2.19.0 - Multiplayer roulette rooms that work, Globed bet results, progress-bar tension
- MULTIPLAYER ROULETTE (Roulette > Multiplayer): create a room (levels source, difficulty, 1-10 rounds, start target, max players, progressive targets, demons only) or join one from the list. Bet into the pot in the lobby - BEFORE the host starts. Ready up (everyone ready = auto start) or the host hits Start game.
  - Each round the bot rolls a level + target %. The FIRST player whose death or completion reaches the target wins the round; progressive rooms raise the target to the winner's % + 1 on a new level. Most round wins takes the pot (+ their own bet again). Majority skip vote re-rolls the level.
  - The room follows you everywhere: round-won / new-round / game-over notifications, and you are taken to each new round level automatically (from inside a level after a short countdown).
  - In-level room scoreboard (top-left, under the bet badge): room code, round, target and standings.
  - WHY IT NEVER WORKED: room rounds were entered from the room popup, so the mod never reported room deaths/completions and the bot could never end a round or pay the pot. Every death and completion on the round level is now reported.
- GLOBED: room-mates sync instantly on every room change, and opening a room invites everyone in your Globed room. Bet results show WIN / LOSS under each player's icon on the progress bar (anyone with the mod).
- NO DEATH GLIMMER while a bet rides on the attempt.
- FULL NUMBERS: the balance counter shows the exact number (no 2.5M) while it counts up or down - bet wins/losses and normal glimmer gains.
- PROGRESS BAR TENSION: with a bet live, a gold marker sits at your target on the bar; in the last 25% the fill heats gold -> orange, a glow pulses faster and faster, sparks fly off the fill tip and the % label warms up; the marker shivers in the last stretch. Crossing the target flashes green with a "LOCKED IN" burst.
- QUITTING with a live bet settles it (win if you already reached the target, otherwise forfeit) instead of leaving it stuck.
- ROULETTE RE-ENABLED (setting renamed so it comes back on for everyone). FIXED: the gold frame corners were turned back on over your background image on every view change - with the photo background the frame stays off and a translucent panel is used instead. The Rarity/Rooms buttons no longer overlap the frame (bottom row: Rarity | Start | Multiplayer, only on the selection screen). Starting a solo roulette no longer overwrites your multiplayer room's level.
- BOT (upload the new gd.js + restart PebbleHost): one bet at a time (a second bet used to overwrite the first and its stake was lost), multi-round room game, bets lock once a game runs, expired/emptied rooms refund bets, room list myBet fix.

## v2.18.0 - Bet badge, real win/loss counter animations, one bet per attempt, difficulty fix
- BET BADGE (top-left, in level): always shows whether this attempt has a bet - "BET 10k @ 60%" (gold, + "AUTO" when re-betting every attempt), "Placing bet..." while the request is in flight, or a dim "No bet". Pops when it changes; hidden in practice.
- WIN ANIMATION: same presentation as earning glimmer, just much bigger - 40-70 glimmer icons burst from the bottom and arc INTO the top-right balance counter; the number turns GREEN and counts up the payout, popping as icons land, with a "+payout" tag under it and a soft green edge glow. WinGamble sound kept.
- LOSS ANIMATION: the gain in reverse - glimmer icons spill OUT of the counter and fall off screen while the number turns RED and quickly counts down the amount you lost, with a "-stake" tag, a small shake, a red edge glow and the low sting.
- REMOVED: the giant glimmer icons (glimmer.png is 256px and was drawn at 0.55-0.85 scale = 140-220px), the light rays, the centre "+payout"/"BET LOST" callouts and the shard burst. All glimmer icons (including the normal award counter) are now sized in pixels (18px counter, 15px flying).
- BET PLACED TOAST: "Bet placed: 10,000 @ 60%" notification on success.
- ONE BET PER ATTEMPT: a second bet is refused while one is live or still being placed ("You already have a bet on this attempt"); double taps and the auto re-bet can no longer stack. Unticking "Re-bet every attempt" still works while a bet is live.
- DIFFICULTY FIX: Harder and Insane were bucketed as Easy-Hard because GD does not keep m_difficulty set on online levels. Buckets now come from the server rating GD itself uses for the face (m_ratingsSum / m_ratings: 4 = Harder, 5 = Insane). Demon tiers now use GD's real key-43 values (3 Easy, 4 Medium, 0 Hard, 5 Insane, 6 Extreme) - Hard demons were landing in Easy/Medium and Extreme demons in Hard/Insane.
- Bot unchanged - no PebbleHost upload needed.

## v2.17.0 - Bet popup rebuilt + win/loss presentation
- BET POPUP REBUILT (330x280): every row has its own band - balance pill (glimmer icon + amount, centred), wide amount input, four equal chips (1k / 10k / 100k / MAX), divider, target row with a rule hint underneath ("Insane: minimum 25%"), centred "Re-bet every attempt" checkbox, and an info pill (payout left, max bet right) above Place bet.
- FIXED: the "%" sign rendered INSIDE the target input box; long payout/balance text could spill because width clamping ran before the text was set (now re-clamped on every update); payout preview now updates live while typing (it only updated on chip taps).
- ROOM "Bet Glimmer" POPUP: the MAX chip sat outside the popup frame - chips are now real equal-width buttons, centred, with a balance pill.
- BET WON: gold screen flash, rotating light rays, a "+payout" pop (with the multiplier, e.g. "BET WON x1.60") that flies into the counter, the icon fountain now actually lands IN the counter (lookup bug), plus a sparkle ring on arrival. WinGamble sound unchanged.
- BET LOST (new): red edge vignette, low-pitched sting, the counter shakes and flashes red while "-stake" drops out of it and the glimmer icon cracks into falling shards, plus a "BET LOST - Died at 42% - needed 60%" callout in the upper middle.
- Manual bets now show the "-amount" tick on the HUD like recurring bets do.
- Bot unchanged - no PebbleHost upload needed.

## v2.16.0 - Popup layout + real checkbox + difficulty detection + win presentation
- BET POPUP LAYOUT: taller frame, every row re-spaced - the win/cap info line now sits CLEAR of the Place bet button (it used to render behind it) and is width-clamped so long payout numbers can never spill; balance line width-clamped under the title too.
- EVERY ATTEMPT IS A REAL CHECKBOX: GD check on/off sprites + label (it used to render as bare text).
- DIFFICULTY DETECTION: the classifier trusted m_demon (a seed value GD leaves stale on ordinary levels) before m_difficulty, mis-tagging Easy-Insane levels as demons. m_difficulty is now authoritative: 1-5 = Easy-Hard/Harder/Insane, 6 = demon by m_demonDifficulty (3-7), 7-10 = explicit demon faces, NA/Auto = Easy-Hard.
- WINS LOOK LIKE NORMAL GAINS: no more WIN banner - the same orbs-style presentation as a regular glimmer award, with MORE icons that all fly INTO the counter (top-right) while the number counts up your win. WinGamble sound still plays on wins.
- Bot unchanged - no PebbleHost upload needed.

## v2.15.0 - Death crash fix + difficulty rules that stick + popup polish
- CRASH FIXED (dying crashed the game): the rewards-screen glimmer counter scanned raw sprite fields that GD leaves stale on death/chest reward layers - it now walks the layer's live children only (crash-proof by construction). A closed chest screen now presents the burst on the scene instead of skipping.
- DIFFICULTY RULES ACTUALLY APPLY NOW: the "Every attempt" preset re-armed each attempt with NO bucket and NO limits - it now clamps to the current level's difficulty rules (cap + target lock) exactly like the popup does.
- BET POPUP: the balance line no longer sits on the title (every row shifted down; title smaller).
- TRUNCATION: bet popup balance, win preview, cap, pause-menu status and the WIN banner payout all compact above 1M (2.5M / 1.2B / 4.9T). Rooms keep the full number.
- Bot unchanged from v2.14 - no PebbleHost upload needed this time.

## v2.14.0 - Bet caps by difficulty + popup polish + balance fix
- NO MORE -1: the bet popup shows "--" and auto-refreshes your balance on open (the HUD already showed "--").
- BET CAPS + TARGET LOCKS BY DIFFICULTY:
  - Easy-Hard: cap 100,000, target locked at 100%
  - Harder: cap 10M, target >= 60%
  - Insane: cap 1B, target >= 25%
  - Easy/Medium Demon: cap 100B, target >= 10%
  - Hard/Insane Demon: cap 1T, target >= 5%
  - Extreme Demon: cap 100T, target >= 3%
  Enforced in the popup (clamped + shown) AND on the bot (bucket validation, 400 over-cap/under-target).
- BET POPUP REDESIGNED: 340x208, clean rows - balance up top, amount input with a MAX chip, evenly spaced 1k/10k/100k chips, target row (locked levels show a gold "100%"), centered Every-attempt checkbox, one info line with the potential win + the difficulty cap, Place bet at the bottom. Everything fits, nothing overlaps.
- Bot: upload the new gd.js + restart PebbleHost (adds the bucket table to bet/start).

## v2.13.0 - Roulette disabled + solo attempt betting
- ROULETTE DISABLED: no entry button on the creator page. (Hidden "roulette-enabled" setting flips it back on.)
- ATTEMPT BETTING: glimmer chip button next to the favorite button on every level page (same size). Opens the bet popup:
  - any amount (free input + 1k/10k/100k/MAX chips),
  - "win at % or above" target (die below it = lose the stake),
  - "Every attempt" checkbox that re-places the same bet each attempt automatically until you untick it or run out of glimmer.
- PAUSE MENU: the same chip + your current bet status ("Bet: 10,000 @ 60%"), change or re-apply from pause.
- PAYOUT: reach target or complete = stake back + stake*target% profit (100% = double). Won bets present the full glimmer award PLUS a much bigger icon fountain, an animated WIN! banner with the payout, and the WinGamble sound.
- REWARDS STACKING: the glimmer line on the rewards screen now anchors under the LOWEST currency row shown (stars/keys/diamonds), no more overlap.
- Bot: bet/start takes targetPercent (escrow unchanged); bet/resolve pays scaled. Upload the new gd.js + restart PebbleHost.

## v2.12.0 - Room roulettes that actually work
- YOUR CUBE IN THE LOBBY: every member row renders their real icon (cube id + colors + glow, SimplePlayer) - Globed player-icon style.
- GROUP ROULETTE WORKS END-TO-END: host taps Start for everyone -> level rolls -> EVERY member's Rooms UI auto-loads that level after ~1s (LevelInfoLayer + download) and the Rooms popup closes itself. Rejoin mid-round with the green play button.
- WIN = BURST + DOUBLE: first finisher takes the pot PLUS their own bet back (bet anything -> win pays double that bet; other bettors' stakes ride on top). Full orbs-style glimmer burst, count-up and jingle on the win.
- BET ANY AMOUNT: Bet popup with a real number input (digits only) + 10k/50k/1M/MAX chips and your balance shown. Balance updates in place.
- SKIP VOTE: while a round is running, anyone taps Skip; majority forces a fresh roll. Progress shows "Skip vote: x/y".
- LEAVING CLOSES THE ROOM UI: Leave (or the room closing) exits the popup back to Browse - no dead lobby on screen.
- GLIMMER UNDER DIAMONDS: on the rewards screen the glimmer line now anchors below the DIAMONDS row (was overlapping it).
- GLIMMER IN THE ICON KIT: shows icon + balance in the garage where all currencies are (next to the diamond shards; fresh balance on open).
- START OVERLAY GONE: the gold corner brackets are hidden with the rest of the frame in photo mode.
- Bot (upload gd.js!): rooms/skip endpoint, rooms/progress pays potWon = pot + own bet, icon data stored per player, friendlier link-required errors.

## v2.11.0 - No-ID setup + Rooms UI rebuild + start fix
- NO DISCORD ID SETTING: removed from mod settings entirely. The mod sends your GD account id; the bot resolves the Discord user you linked with /gd link in Discord. Link once, everything works (rooms, betting, glimmer).
- ROOMS UI REBUILT (Globed-style): fixed pixel columns - labels left, arrows in two aligned columns, values centered (the old rows had arrows overlapping text); tabs clear of content; Browse rows are translucent cells (code, host, x/y, Join); Lobby is a Globed player list: 8 fixed rows with [host]/ready-checks/bets, Pot left + full balance right, Bet | Ready/Start | Leave in one bottom row. Nothing overlaps.
- START FIXED: (a) the host counts as ready from creation, so "everyone ready" auto-start can actually fire; (b) "Demons only" is now a SOURCE - the old mapping sent rarity=demon, which rolled nothing and made start fail with 'no level rolled'.
- LOBBY NEVER REBUILDS ON A TIMER: member changes only show/hide the fixed rows (root cause of frozen enlarged arrows is gone for good).
- Roulette chrome: the gold corner art is hidden with the rest of the frame in photo mode; the GD Level Roulette title is fully visible again.
- Bot: upload the new gd.js (identity resolver on every rooms/bet/run handler + host auto-ready + link-required messages).

## v2.10.0 - Room lobby overhaul (Globed-style)
- FRAME CHROME GONE: with the photo active, GD-Roulette's entire frame (gold beams, bottom blocks, fills) is hidden - the photo IS the background. Title text dimmed to keep it subtle.
- LOBBY: ready/unready for members, host gets "Start for everyone" (force start + auto-ready). Live member list: host tag, ready state, bets, (you). Pot + your FULL balance (no truncation) at the top.
- BETTING: "Bet Glimmer" in the lobby drops into the shared pot (escalating: 10k, 50k, 250k, 1M, 5M, 25M). Winners split the pot when the round resolves.
- OWNERSHIP: can't join your own room; leaving transfers host to the next member (or closes it); host leaving mid-lobby hands the crown over.
- ARROW STICKING FIXED: lobby polls no longer rebuild the page (that's what froze the pressed/enlarged arrows) - labels update in place; page rebuilds only when membership changes.
- (Bot: rooms/ready, rooms/start, rooms/bet endpoints + host transfer - upload the new gd.js and restart PebbleHost.)

## v2.9.1
- CREATE-ROOM CRASH FIXED (your log, PC 0x0 in refresh): refresh() cleared the scroll content - which DELETES the list menu after the first populate - then touched the freed pointer. Now detached in the safe order, nulled, rebuilt. Any rooms action is safe now.
- Yellow frame over the photo: the whole gold frame chrome (border beams, bottom blocks, fills) is tinted dark + 53% opacity when the photo is active.
- Rooms popup: tabs moved below the "Sorrow Rooms" title (no more overlap); source/difficulty/max-players steppers use REAL arrow sprites (one frame + flip - the pink checkerboard blocks were missing frame names); Progressive and Demons are real GD checkboxes now.
- START raised clear of the bottom beam.

## v2.9.0
- EMPTY ROOMS POPUP FIXED (the real one): geode's plain Popup class has NO setup() hook - our setup() (tabs, pages, list, everything) was NEVER CALLED; the popup showed a bare grey box. create() now invokes setup() explicitly after init().
- Difficulty N/A FIXED: applyDifficulty now writes GD's real face encoding (m_ratings denominator 10, numerator 10*face) and demon codes 3..6 matching the display node's consumer - Easy..Insane and all demon tiers show the correct face immediately.
- Stereo Madness song FIXED: songId/officialIndex now parsed from the bridge payload (the bot now sends them too - re-upload gd.js) so custom songs resolve by ID and official tracks use the right track index.
- START button rebuilt as a native ButtonSprite: text auto-sized and centered (the raw RL_longBtn sprite mis-scaled its label on wide screens).
- Rooms/Rarity buttons moved to the BOTTOM-LEFT corner (stacked, clear of START); labels re-fit their background via updateBGImage when text changes (Room <code>).

## v2.8.0 - Roulette UI overhaul
- BACKGROUND FIXED: the opaque RL_listBorderBackground fills inside the roulette frame were covering the photo - they now render translucent (55%) so the photo shows through while text stays readable.
- NEW bottom action bar: Rooms + Rarity sit in one RowLayout bar at the bottom center (no more floating buttons over the frame borders). LIVE LABELS: the Rooms button reads "Room 24GZ" while in a room; the Rarity button shows the current filter ("Rarity: Epic") and updates the moment you cycle it.
- CREATE ROOMS: an empty browse page now shows a big "+ Create Room" button that jumps straight to the Create tab (creating a room was buried in a tab before).

## v2.7.2
- ORB-FOUNTAIN BURST: all glimmer icons are visible from frame 0, clustered at the spawn point; they pop radially outward at once (upward-biased), then fly into the counter in quick succession (bounce + settle unchanged). No invisibility, no waiting line.

## v2.7.1
- THE BOTTOM LINE REMOVED: the flying icons spawned SPREAD ACROSS A ROW at the bottom and waited visible up to 1.8s for their staggered turns - that was the "line of glimmer icons". They now all spawn on one spot (tiny random scatter) and stay INVISIBLE until their own flight begins.
- '+Glimmer' chest line RESTORED (was removed by misunderstanding).

## v2.7.0
- Death-sound FIX: death-rewards screens built without create()/init (deserialized) skipped the payout entirely - the death payout is now also caught in onEnter (runs for every CurrencyRewardLayer regardless of construction), with a per-instance guard against double-crediting.
- Sound volume amplified (raw channel 1.5x, FMOD supports >1.0).
- Flight rebuilt vanilla-style: icons burst UP, fall, BOUNCE once just below the counter, then settle into it (chained beziers). More icons: 10-18 (was 6-12), tighter stagger.
- '+Glimmer' chest line removed entirely.
- Profile: glimmer icon nudged closer to the number (left).

## v2.6.0
- Glimmer counter placed UNDER the orbs counter (same column, one row down) instead of floating above-left.
- FLYING GLIMMER VISIBLE: the burst icons now live in the same coordinate space as the counters (v2.5.0 drew them in the layer's space, which is offset from the counter container - off-screen). They now burst from the screen's bottom center and arc into the glimmer counter.
- DEATH pays glimmer: dying (non-anticheat) credits a random 10,000-25,000 with the same vanilla-style sequence + jingle on the death rewards screen.
- RACE FIXED: when the award response lands after the rewards screen already opened (common - that is why the burst often never ran), the EndLevelLayer poll now plays the sequence on the live screen immediately.

## v2.5.0
- THE REAL ORB ANIMATION: glimmer now uses GD's own rewards screen (CurrencyRewardLayer) - the same screen where orbs count up. A glimmer counter joins the orb counter with the SAME icon size (scale derived from the orb sprite), same label scale, and the vanilla pulseSprite pulse at every icon arrival; a stream of glimmer icons arcs up into it and the earned amount counts 0 -> gain exactly like orbs. The counter lives inside the layer so its leaving animation is the vanilla one.
- Sound now plays at the start of every vanilla sequence (level completion + chest), plus the HUD fallback presentations - i.e. on EVERY glimmer gain.
- Profile stat: icon is now on the RIGHT of the number.
- End screen no longer draws its own custom counter (the rewards screen owns the presentation); HUD volley remains only as fallback for paths without the rewards screen.

## v2.4.2
- Sound REBUILT to be deterministic: the jingle bytes are read with std::ifstream and decoded by FMOD from MEMORY (FMOD_OPENMEMORY) - bypassing FMOD's Android file layer entirely (the thing that made every file-path attempt silent) and the playEffect pitch-0 bug. Raw channel volume 1.0 (max). playEffect(pitch=1) kept as logged fallback; every step result logged.

## v2.4.1
- NEW GD-style glimmer icon: faceted gem, thick outline, purple gradient (256x256, true transparency) - used everywhere (profile stat, counter, volley, chest line).
- Award counter now behaves like the orb counter: appears on gain, shows the volley + count-up, then FADES OUT - it no longer sticks in the top-right corner.
- Volley icons doubled in size (~0.85 scale) with a wider burst radius.

## v2.4.0
- Profile glimmer stat FIXED: the 2.2 stats row is a RowLayout (NodeIDs stats-menu) - the stat now joins the layout as the FIRST item (far left, in row). No more drifting down/right; compact formatting (5T, 12.4M) keeps it row-sized.
- Orbs-style award presentation (AwardFx.hpp): on every glimmer gain (level completion, chest, bet) a glimmer counter pops into the TOP-RIGHT corner (like the orb counter), a volley of glimmer icons flies into it, and the balance counts up.
- Sound FIXED: the jingle now goes through GD's own sfx path (FMODAudioEngine preloadEffect + playEffect with the mod resource path) instead of a raw FMOD createSound that stayed silent on Android; result logged once.
- End screen keeps the reward flow: poll claims the pending award and presents it on the end screen; scene fallback after 6s.

## v2.3.0
- Rooms list FIXED: rows were placed below the ScrollLayer's content origin and clipped invisible - now positioned in the content box properly, Join buttons scroll with rows.
- Photo background FIXED: GD's Android build cannot decode JPG - the background now ships as PNG (sorrow_bg.png). This is why it never appeared.
- Glimmer animation on EVERY level completion: the end-screen reward poll ran only on roulette-goal screens; now every end screen claims the payout (icon burst + fly-away, orbs-style).
- Profile glimmer stat: moved to the FAR LEFT of the stats row (row shifts right to make room); own profile never loses the row; placeholder shows "..." until the balance arrives; profile responses logged.

## v2.2.1
- CRASH FIX: Reset button crashed when the photo background was active (the repeating-bg node was skipped but still referenced). The node now always exists, just hidden.
- ROOT FIX for vanishing UI: bridge callbacks now always run on the main thread (rooms list, profile glimmer row, bets could silently fail to render from a worker thread).
- Rooms popup: loading / offline / empty states are explicit; refresh is fetch-callback driven instead of a timed guess; fixed a dangling-pointer bug in Join buttons with 2+ rooms.
- Profile page and Rooms popup now use WeakRef guards so late responses to closed screens can't crash.

## v2.2.0
- Level roulette play FIXED: the bot's level id arrives as a string and difficulties are capitalized - both now parsed correctly (this was the "Challenge" / download-failure bug).
- Roulette background is the custom photo (sorrow_bg.jpg) with a dark overlay; vanilla stripes removed when it loads.
- Rooms/Rarity buttons moved up off the border bar.
- Glimmer now pays with an orbs-style burst animation on the end screen (icon flies to the HUD with the feebls sound).
- Chest rewards (daily/weekly) pay Glimmer too, with a +Glimmer line under the orbs counter.
- Profile Glimmer row re-anchored to the vanilla stats-menu (BetterInfo's approach).
- Sorrow Rooms popup redesigned: Browse/Create tabs, cleaner list with Join buttons, green/gray toggle states.

## v2.1.1
- Rotated the bridge token (worker secret + hardcoded mod token updated together).

## v2.1.0
- Fixed all missing-texture (checkerboard) sprites: mod resources now install to the correct search path and sprite frames are preloaded before any UI is built.
- Profile Glimmer row rebuilt: detects the stats row layout-independently and sits right after the last stat icon (demons).
- Glimmer line added to the profile info popup (Stars/Moons/Diamonds list).
- Transparent Glimmer icon (white background removed).
- Rooms/Rarity buttons restyled as proper GD buttons in the bottom corners.
- Sorrow Rooms popup redesigned: dark theme, real selector rows (source / difficulty / max players), toggle buttons, per-room Join buttons, active-room strip with Leave.

# Changelog

## v1.0.0
- Initial release: roulette bridge, Globed group roulette, death stats, HUD.

## v1.1.0
- Fixed crash on level start (missing GEODE_MOD_ID made "_spr" return null).
- Glimmer balance shown in the HUD with an icon; payout toasts on roulette wins.
- New /bridge/sorrow/balance endpoint integration.
