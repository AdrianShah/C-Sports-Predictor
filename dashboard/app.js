// C-Sports-Predictor dashboard. Static page: reads the model's picks and
// results straight from the profile repo, so it is always current without a
// redeploy. `?data=<url>/` points it at another copy of the data folder.

const DATA_URL =
  new URLSearchParams(location.search).get("data") ||
  "https://raw.githubusercontent.com/AdrianShah/AdrianShah/main/data/";
const PAGE_SIZE = 120;
const TZ = "America/Toronto";
const SPORTS = { soccer: "Soccer", nba: "NBA", nhl: "NHL" };

const state = { view: "upcoming", sport: "all", query: "", sort: "time", adrianOnly: false, shown: PAGE_SIZE };
let games = [];

// MARK: Data

async function getJson(name, fallback) {
  try {
    const res = await fetch(DATA_URL + name, { cache: "no-cache" });
    if (res.status === 404) return fallback;
    if (!res.ok) throw new Error(`${name}: HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.warn(err);
    return fallback;
  }
}

const norm = (s) => (s || "").toLowerCase().trim();

/** One record per model pick, with its outcome and Adrian's pick on the same game. */
function buildGames(modelPicks, modelResults, myPicks) {
  const mine = new Map(myPicks.map((p) => [p.eventId, p]));
  return modelPicks.map((p) => {
    const r = modelResults[p.eventId];
    const probs = { a: p.pA ?? 0, draw: p.pDraw ?? 0, b: p.pB ?? 0 };
    const side = norm(p.pick) === norm(p.a) ? "a" : norm(p.pick) === norm(p.b) ? "b" : "draw";
    const start = Date.parse(p.scheduledAt);

    let outcome = "pending";
    if (r?.status) outcome = "void";
    else if (Date.parse(p.lockedAt) >= start) outcome = "void";
    else if (r?.winner) outcome = norm(r.winner) === norm(p.pick) ? "correct" : "incorrect";

    const w = norm(r?.winner);
    const actual = !r?.winner ? null : w === norm(p.a) ? "a" : w === norm(p.b) ? "b" : w === "draw" ? "draw" : null;

    const m = mine.get(p.eventId);
    const adrianInTime = m && (m.legacy || Date.parse(m.lockedAt) < start);
    const adrian = m && {
      pick: m.pick,
      differs: norm(m.pick) !== norm(p.pick),
      outcome: !adrianInTime || m.void ? "void" : r?.winner ? (norm(r.winner) === norm(m.pick) ? "correct" : "incorrect") : "pending",
    };

    return {
      id: p.eventId, sport: p.sport, competition: p.competition || "", a: p.a, b: p.b,
      aLogo: p.aLogo, bLogo: p.bLogo, start, pick: p.pick, side, prob: probs[side], probs,
      outcome, actual, score: r?.score || "", adrian,
    };
  });
}

const decided = (g) => g.outcome === "correct" || g.outcome === "incorrect";

function brierOf(list) {
  const scored = list.filter((g) => decided(g) && g.actual);
  if (!scored.length) return null;
  const sum = scored.reduce(
    (s, g) => s + ["a", "draw", "b"].reduce((t, k) => t + (g.probs[k] - (k === g.actual ? 1 : 0)) ** 2, 0),
    0
  );
  return sum / scored.length;
}

function recordOf(list) {
  const d = list.filter(decided);
  const correct = d.filter((g) => g.outcome === "correct").length;
  return { n: d.length, correct, acc: d.length ? correct / d.length : null, brier: brierOf(list) };
}

// MARK: Formatting

const pct = (x, digits = 0) => (x == null ? "–" : `${(x * 100).toFixed(digits)}%`);
const fmt = (n) => n.toLocaleString("en-US");
const dayKey = new Intl.DateTimeFormat("en-CA", { timeZone: TZ, year: "numeric", month: "2-digit", day: "2-digit" });
const dayLabel = new Intl.DateTimeFormat("en-US", { timeZone: TZ, weekday: "long", month: "long", day: "numeric" });
const timeLabel = new Intl.DateTimeFormat("en-US", { timeZone: TZ, hour: "numeric", minute: "2-digit" });

/** Builds DOM; strings become text nodes, so team names are never parsed as HTML. */
function el(tag, attrs = {}, ...children) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (v == null || v === false) continue;
    if (k === "class") node.className = v;
    else if (k === "style") node.style.cssText = v;
    else if (k.startsWith("on")) node.addEventListener(k.slice(2), v);
    else node.setAttribute(k, v === true ? "" : v);
  }
  for (const c of children.flat()) if (c != null && c !== false) node.append(c);
  return node;
}

const svgEl = (tag, attrs = {}) => {
  const node = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, v);
  return node;
};

// MARK: Tooltip

const tip = document.getElementById("tooltip");

function showTip(evt, value, label) {
  tip.replaceChildren(el("div", { class: "v" }, value), el("div", { class: "k" }, label));
  tip.hidden = false;
  const r = evt.target.getBoundingClientRect();
  const x = evt.clientX ?? r.left + r.width / 2;
  const y = evt.clientY ?? r.top;
  const w = tip.offsetWidth;
  tip.style.left = `${Math.min(Math.max(8, x - w / 2), innerWidth - w - 8)}px`;
  tip.style.top = `${Math.max(8, y - tip.offsetHeight - 12)}px`;
}
const hideTip = () => (tip.hidden = true);
// Content moves under a still pointer when scrolling, so pointerleave never fires.
addEventListener("scroll", hideTip, { passive: true });

function withTip(node, value, label) {
  node.addEventListener("pointermove", (e) => showTip(e, value, label));
  node.addEventListener("pointerleave", hideTip);
  node.addEventListener("focus", (e) => showTip(e, value, label));
  node.addEventListener("blur", hideTip);
  return node;
}

// MARK: Summary tiles

function renderTiles() {
  const all = recordOf(games);
  const upcoming = games.filter((g) => g.outcome === "pending").length;

  const shared = games.filter((g) => g.adrian && decided(g) && (g.adrian.outcome === "correct" || g.adrian.outcome === "incorrect"));
  const adrianWins = shared.filter((g) => g.adrian.outcome === "correct").length;
  const modelWins = shared.filter((g) => g.outcome === "correct").length;

  const tile = (label, value, note, hero = false) =>
    el("div", { class: `tile${hero ? " hero" : ""}` }, el("div", { class: "label" }, label), el("div", { class: "value" }, value), note && el("div", { class: "note" }, note));

  document.getElementById("tiles").replaceChildren(
    tile("Accuracy", pct(all.acc), all.n ? `${fmt(all.correct)} of ${fmt(all.n)} decided picks` : "No results yet", true),
    tile("Brier score", all.brier == null ? "–" : all.brier.toFixed(3), "Lower is better"),
    tile("Upcoming picks", fmt(upcoming), `${fmt(games.length)} picks in total`),
    tile(
      "Adrian vs the model",
      shared.length ? `${adrianWins}–${modelWins}` : "–",
      shared.length ? `Correct picks on ${shared.length} shared games` : "No shared games decided yet"
    )
  );
}

// MARK: Game list

function filtered() {
  const q = norm(state.query);
  let list = games.filter((g) => {
    if (state.view === "upcoming" ? g.outcome !== "pending" : g.outcome === "pending") return false;
    if (state.sport !== "all" && g.sport !== state.sport) return false;
    if (state.adrianOnly && !g.adrian) return false;
    if (q && !norm(`${g.a} ${g.b} ${g.competition}`).includes(q)) return false;
    return true;
  });
  const byTime = state.view === "upcoming" ? (x, y) => x.start - y.start : (x, y) => y.start - x.start;
  list.sort(state.sort === "confidence" ? (x, y) => y.prob - x.prob || byTime(x, y) : byTime);
  return list;
}

function teamLine(name, logo) {
  return el(
    "div",
    { class: "team" },
    logo ? el("img", { src: logo, alt: "", loading: "lazy", width: 20, height: 20 }) : el("i", { class: "noimg" }),
    el("span", {}, name)
  );
}

function probBar(g) {
  const parts = [
    ["home", g.probs.a, `${g.a} win`],
    ...(g.sport === "soccer" ? [["draw", g.probs.draw, "Draw"]] : []),
    ["away", g.probs.b, `${g.b} win`],
  ];
  return el(
    "div",
    { class: "bar", role: "img", "aria-label": parts.map(([, p, l]) => `${l} ${pct(p)}`).join(", ") },
    parts.map(([cls, p, label]) =>
      withTip(
        // A label goes inside a segment only when it clearly fits.
        el("div", { class: `seg-bar ${cls}`, style: `flex: ${Math.max(p, 0.001)} 1 0` }, p >= 0.17 ? pct(p) : ""),
        pct(p, 1),
        label
      )
    )
  );
}

function resultCell(g) {
  if (g.outcome === "pending") {
    return el("div", { class: "result" }, el("span", { class: "score" }, g.start <= Date.now() ? "Awaiting result" : ""));
  }
  const mark =
    g.outcome === "correct"
      ? el("span", { class: "mark good" }, "✓ Right")
      : g.outcome === "incorrect"
        ? el("span", { class: "mark bad" }, "✗ Wrong")
        : el("span", { class: "mark void" }, "Void");
  return el("div", { class: "result" }, mark, g.score && el("span", { class: "score" }, g.score));
}

function gameRow(g) {
  const pickName = g.side === "draw" ? "Draw" : g.pick;
  return el(
    "article",
    { class: "game" },
    el("div", { class: "time" }, timeLabel.format(g.start)),
    el("div", { class: "match" }, teamLine(g.a, g.aLogo), teamLine(g.b, g.bLogo), el("div", { class: "comp" }, g.competition)),
    probBar(g),
    el(
      "div",
      { class: "pick" },
      el("b", {}, pickName),
      " ",
      el("span", { class: "pct" }, pct(g.prob)),
      g.adrian &&
        el("span", { class: `adrian${g.adrian.differs ? " differs" : ""}` }, `Adrian: ${norm(g.adrian.pick) === "draw" ? "Draw" : g.adrian.pick}`)
    ),
    resultCell(g)
  );
}

function renderList() {
  const list = filtered();
  const root = document.getElementById("list");
  const more = document.getElementById("more");
  if (!list.length) {
    const msg = games.length
      ? "No games match these filters."
      : "The model's first picks arrive with the next fixtures run (09:00 and 21:00 UTC).";
    root.replaceChildren(el("p", { class: "empty" }, msg));
    more.hidden = true;
    return;
  }

  const nodes = [];
  let lastDay = null;
  for (const g of list.slice(0, state.shown)) {
    const key = dayKey.format(g.start);
    if (state.sort === "time" && key !== lastDay) {
      nodes.push(el("h3", { class: "day" }, dayLabel.format(g.start)));
      lastDay = key;
    }
    nodes.push(gameRow(g));
  }
  root.replaceChildren(...nodes);
  more.hidden = list.length <= state.shown;
  more.textContent = `Show more (${fmt(list.length - state.shown)} left)`;
}

// MARK: Calibration chart

function calibrationBuckets(list) {
  const edges = [0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0001];
  const buckets = edges.slice(0, -1).map((lo, i) => ({ lo, hi: edges[i + 1], n: 0, hits: 0, conf: 0 }));
  for (const g of list) {
    if (!decided(g)) continue;
    const b = buckets.find((x) => g.prob >= x.lo && g.prob < x.hi) || buckets[0];
    b.n++;
    b.conf += g.prob;
    if (g.outcome === "correct") b.hits++;
  }
  return buckets
    .filter((b) => b.n > 0)
    .map((b) => ({ ...b, label: `${Math.round(b.lo * 100)}–${Math.min(100, Math.round(b.hi * 100))}%`, rate: b.hits / b.n, conf: b.conf / b.n }));
}

function renderCalibration() {
  const list = games.filter((g) => state.sport === "all" || g.sport === state.sport);
  const buckets = calibrationBuckets(list);
  const chart = document.getElementById("calibration");
  const table = document.getElementById("calib-table");
  if (!buckets.length) {
    chart.replaceChildren(el("p", { class: "empty" }, "Appears once picks have been decided."));
    table.replaceChildren();
    return;
  }

  const W = 640, H = 240, L = 40, R = 8, T = 10, B = 28;
  const plotW = W - L - R, plotH = H - T - B;
  const band = plotW / buckets.length;
  const y = (v) => T + plotH * (1 - v);
  const svg = svgEl("svg", { viewBox: `0 0 ${W} ${H}`, role: "img", "aria-label": "Hit rate by model confidence" });

  for (const v of [0, 0.25, 0.5, 0.75, 1]) {
    svg.append(svgEl("line", { class: v === 0 ? "baseline" : "gridline", x1: L, x2: W - R, y1: y(v), y2: y(v) }));
    const t = svgEl("text", { class: "tick", x: L - 8, y: y(v) + 4, "text-anchor": "end" });
    t.textContent = `${v * 100}%`;
    svg.append(t);
  }

  buckets.forEach((b, i) => {
    const cx = L + band * i + band / 2;
    const w = Math.min(24, band * 0.5);
    const top = y(b.rate);
    const h = Math.max(0, y(0) - top);
    const r = Math.min(4, h / 2);
    // Rounded data-end, square at the baseline.
    const d = `M${cx - w / 2},${y(0)} V${top + r} Q${cx - w / 2},${top} ${cx - w / 2 + r},${top} H${cx + w / 2 - r} Q${cx + w / 2},${top} ${cx + w / 2},${top + r} V${y(0)} Z`;

    const hit = svgEl("rect", { class: "hit", x: cx - band / 2 + 2, y: T, width: band - 4, height: plotH, tabindex: 0 });
    withTip(hit, `${pct(b.rate)} won`, `${b.label} confidence · ${fmt(b.n)} picks · model said ${pct(b.conf)} on average`);
    svg.append(hit, svgEl("path", { class: "col", d }));
    svg.append(svgEl("circle", { class: "expected", cx, cy: y(b.conf), r: 4.5, "pointer-events": "none" }));

    const label = svgEl("text", { class: "tick", x: cx, y: H - 8, "text-anchor": "middle" });
    label.textContent = b.label;
    svg.append(label);
  });
  chart.replaceChildren(svg);

  table.replaceChildren(
    el(
      "table",
      {},
      el("thead", {}, el("tr", {}, el("th", {}, "Confidence"), el("th", {}, "Picks"), el("th", {}, "Model said"), el("th", {}, "Actually won"))),
      el("tbody", {}, buckets.map((b) => el("tr", {}, el("td", {}, b.label), el("td", {}, fmt(b.n)), el("td", {}, pct(b.conf, 1)), el("td", {}, pct(b.rate, 1)))))
    )
  );
}

function renderSports() {
  const rows = Object.entries(SPORTS).map(([key, name]) => {
    const list = games.filter((g) => g.sport === key);
    const r = recordOf(list);
    return el(
      "tr",
      {},
      el("td", {}, name),
      el("td", {}, fmt(list.length)),
      el("td", {}, r.n ? `${fmt(r.correct)}/${fmt(r.n)}` : "–"),
      el("td", {}, pct(r.acc, 1)),
      el("td", {}, r.brier == null ? "–" : r.brier.toFixed(3))
    );
  });
  document.getElementById("sports-table").replaceChildren(
    el(
      "table",
      {},
      el("thead", {}, el("tr", {}, el("th", {}, "Sport"), el("th", {}, "Picks"), el("th", {}, "Record"), el("th", {}, "Accuracy"), el("th", {}, "Brier"))),
      el("tbody", {}, rows)
    )
  );
}

// MARK: Controls

function bindSeg(id, key, attr) {
  const root = document.getElementById(id);
  root.addEventListener("click", (e) => {
    const btn = e.target.closest("button");
    if (!btn) return;
    state[key] = btn.dataset.value;
    state.shown = PAGE_SIZE;
    for (const b of root.querySelectorAll("button")) b.setAttribute(attr, String(b === btn));
    render();
  });
}

function render() {
  renderList();
  renderCalibration();
}

async function main() {
  bindSeg("view", "view", "aria-selected");
  bindSeg("sport", "sport", "aria-checked");
  document.getElementById("search").addEventListener("input", (e) => {
    state.query = e.target.value;
    state.shown = PAGE_SIZE;
    renderList();
  });
  document.getElementById("sort").addEventListener("change", (e) => {
    state.sort = e.target.value;
    renderList();
  });
  document.getElementById("adrian").addEventListener("change", (e) => {
    state.adrianOnly = e.target.checked;
    state.shown = PAGE_SIZE;
    renderList();
  });
  document.getElementById("more").addEventListener("click", () => {
    state.shown += PAGE_SIZE;
    renderList();
  });

  document.getElementById("list").replaceChildren(el("p", { class: "empty" }, "Loading picks…"));
  const [modelPicks, modelResults, myPicks] = await Promise.all([
    getJson("model_picks.json", { generatedAt: null, picks: [] }),
    getJson("model_results.json", {}),
    getJson("picks.json", { picks: [] }),
  ]);
  games = buildGames(modelPicks.picks || [], modelResults, myPicks.picks || []);

  renderTiles();
  renderSports();
  render();
  if (modelPicks.generatedAt) {
    document.getElementById("updated").textContent =
      `Picks last generated ${new Date(modelPicks.generatedAt).toLocaleString("en-US", { timeZone: TZ, dateStyle: "medium", timeStyle: "short" })} (Toronto).`;
  }
}

main();
