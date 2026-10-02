#!/usr/bin/env node
// Downloads finished games from ESPN's public scoreboard API into
// history/<sport>/<year>.csv, the input of the C++ model.
//
// ESPN only answers one date per request, so this walks day by day. Soccer
// uses the "all" feed: every league for a date in one response, with the same
// team names the profile repo's fixtures use.
//
//   node tools/backfill.mjs                          # all sports, from each default start date
//   node tools/backfill.mjs --sport nba --from 2024-10-01 --to 2025-06-30
//   node tools/backfill.mjs --incremental            # last stored day (minus 3) to yesterday
//
// Requires Node 18+ (global fetch). No dependencies.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const HISTORY = path.join(ROOT, "history");
const LEAGUES_FILE = path.join(ROOT, "config", "espn_leagues.json");
const COMPETITIONS_FILE = path.join(ROOT, "config", "competitions.json");
const BASE = "https://site.api.espn.com/apis/site/v2/sports";
const COLUMNS = ["date", "season", "league", "home", "away", "home_score", "away_score", "neutral", "espn_id"];

const SPORTS = {
  soccer: { feed: "soccer/all", start: "2022-07-01" },
  // Regular season (2), postseason (3) and play-in (5); preseason and all-star games are skipped.
  nba: { feed: "basketball/nba", start: "2022-10-01", seasonTypes: [2, 3, 5] },
  nhl: { feed: "hockey/nhl", start: "2022-10-01", seasonTypes: [2, 3] },
};

// MARK: Arguments

function parseArgs(argv) {
  const args = { sport: "all", concurrency: 6, incremental: false };
  for (let i = 0; i < argv.length; i++) {
    const key = argv[i];
    if (key === "--incremental") args.incremental = true;
    else if (["--sport", "--from", "--to", "--concurrency"].includes(key)) args[key.slice(2)] = argv[++i];
    else throw new Error(`unknown argument ${key}`);
  }
  args.concurrency = Number(args.concurrency);
  const sports = args.sport === "all" ? Object.keys(SPORTS) : [args.sport];
  for (const s of sports) if (!SPORTS[s]) throw new Error(`unknown sport ${s}`);
  return { ...args, sports };
}

// MARK: Dates (UTC calendar days)

const isoDay = (d) => d.toISOString().slice(0, 10);
const espnDay = (day) => day.replaceAll("-", "");
const addDays = (day, n) => isoDay(new Date(Date.parse(`${day}T00:00:00Z`) + n * 86_400_000));

function daysBetween(from, to) {
  const out = [];
  for (let d = from; d <= to; d = addDays(d, 1)) out.push(d);
  return out;
}

// MARK: HTTP

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function get(url, attempts = 5) {
  for (let i = 1; ; i++) {
    let retryable = true;
    try {
      const res = await fetch(url, { headers: { "user-agent": "picks-model-backfill" } });
      if (res.ok) return await res.json();
      retryable = res.status === 429 || res.status >= 500;
      throw new Error(`HTTP ${res.status} for ${url}`);
    } catch (err) {
      if (!retryable || i >= attempts) throw err;
      await sleep(1000 * 2 ** i);
    }
  }
}

/** Runs `task` over `items` with at most `limit` in flight, preserving order. */
async function pool(items, limit, task) {
  const results = new Array(items.length);
  let next = 0;
  const worker = async () => {
    while (next < items.length) {
      const i = next++;
      results[i] = await task(items[i], i);
    }
  };
  await Promise.all(Array.from({ length: Math.min(limit, items.length) }, worker));
  return results;
}

// MARK: CSV

const csvField = (v) => {
  const s = String(v ?? "");
  return /[",\r\n]/.test(s) ? `"${s.replaceAll('"', '""')}"` : s;
};

function parseCsv(text) {
  const rows = [];
  let row = [], field = "", quoted = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (quoted) {
      if (c !== '"') field += c;
      else if (text[i + 1] === '"') { field += '"'; i++; }
      else quoted = false;
    } else if (c === '"') quoted = true;
    else if (c === ",") { row.push(field); field = ""; }
    else if (c === "\n") { row.push(field); rows.push(row); row = []; field = ""; }
    else if (c !== "\r") field += c;
  }
  if (field || row.length) { row.push(field); rows.push(row); }
  return rows;
}

function readSport(sport) {
  const dir = path.join(HISTORY, sport);
  const games = new Map();
  if (!fs.existsSync(dir)) return games;
  for (const file of fs.readdirSync(dir).filter((f) => f.endsWith(".csv"))) {
    const [header, ...rows] = parseCsv(fs.readFileSync(path.join(dir, file), "utf8"));
    for (const r of rows) {
      const g = Object.fromEntries(header.map((h, i) => [h, r[i]]));
      games.set(g.espn_id, g);
    }
  }
  return games;
}

/** Writes one file per calendar year; returns how many files changed. */
function writeSport(sport, games) {
  const dir = path.join(HISTORY, sport);
  fs.mkdirSync(dir, { recursive: true });
  const byYear = new Map();
  const sorted = [...games.values()].sort((a, b) => a.date.localeCompare(b.date) || a.espn_id.localeCompare(b.espn_id));
  for (const g of sorted) {
    const year = g.date.slice(0, 4);
    if (!byYear.has(year)) byYear.set(year, [COLUMNS.join(",")]);
    byYear.get(year).push(COLUMNS.map((c) => csvField(g[c])).join(","));
  }
  let changed = 0;
  for (const [year, lines] of byYear) {
    const file = path.join(dir, `${year}.csv`);
    const text = lines.join("\n") + "\n";
    if (fs.existsSync(file) && fs.readFileSync(file, "utf8") === text) continue;
    fs.writeFileSync(file, text);
    changed++;
  }
  return changed;
}

// MARK: ESPN events

function loadJson(file, fallback) {
  return fs.existsSync(file) ? JSON.parse(fs.readFileSync(file, "utf8")) : fallback;
}

const leagueIdOf = (event) => (event.uid || "").match(/l:(\d+)/)?.[1] || null;

async function resolveLeague(leagues, id, sampleEventId) {
  if (!leagues[id]) {
    const summary = await get(`${BASE}/soccer/all/summary?event=${sampleEventId}`);
    const l = summary.header?.league || {};
    leagues[id] = { name: l.name || `League ${id}`, slug: l.slug || null };
    console.log(`  new league ${id}: ${leagues[id].name} (${leagues[id].slug})`);
  }
  return leagues[id];
}

/** Same rules as the C++ predict filter (config/competitions.json), plus the slug pattern. */
function excluded(league, { exclude, slugPattern }) {
  const name = league.name.toLowerCase();
  if (slugPattern && league.slug && slugPattern.test(league.slug)) return true;
  return exclude.some((x) => (x.startsWith("=") ? name === x.slice(1) : name.includes(x)));
}

/** A history row for a finished game, or null if it shouldn't be stored. */
function toRow(event, league) {
  const comp = event.competitions?.[0];
  const type = comp?.status?.type || event.status?.type;
  if (!comp || !type?.completed || /CANCEL|POSTPONE|ABANDON|FORFEIT|SUSPEND/.test(type.name || "")) return null;
  const home = comp.competitors?.find((c) => c.homeAway === "home");
  const away = comp.competitors?.find((c) => c.homeAway === "away");
  const hs = Number(home?.score?.value ?? home?.score);
  const as = Number(away?.score?.value ?? away?.score);
  const homeName = home?.team?.displayName;
  const awayName = away?.team?.displayName;
  if (!homeName || !awayName || !Number.isInteger(hs) || !Number.isInteger(as)) return null;
  return {
    date: new Date(event.date).toISOString().replace(".000Z", "Z"),
    season: event.season?.year ?? "",
    league,
    home: homeName,
    away: awayName,
    home_score: hs,
    away_score: as,
    neutral: comp.neutralSite ? 1 : 0,
    espn_id: event.id,
  };
}

async function fetchDay(sport, day, ctx) {
  const { feed, seasonTypes } = SPORTS[sport];
  const data = await get(`${BASE}/${feed}/scoreboard?dates=${espnDay(day)}&limit=1000`);
  const rows = [];
  for (const event of data.events || []) {
    let league = sport;
    if (sport === "soccer") {
      const id = leagueIdOf(event);
      const info = id ? await resolveLeague(ctx.leagues, id, event.id) : { name: "", slug: null };
      if (excluded(info, ctx)) continue;
      league = info.slug || (id ? `l${id}` : "");
    } else if (seasonTypes && !seasonTypes.includes(event.season?.type)) {
      continue;
    }
    const row = toRow(event, league);
    if (row) rows.push(row);
  }
  return rows;
}

// MARK: Main

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const yesterday = addDays(isoDay(new Date()), -1);
  const leagues = loadJson(LEAGUES_FILE, {});
  const competitions = loadJson(COMPETITIONS_FILE, { excludeCompetitions: [] });
  const ctx = {
    leagues,
    exclude: competitions.excludeCompetitions.map((x) => x.toLowerCase()),
    slugPattern: competitions.excludeLeagueSlugPattern ? new RegExp(competitions.excludeLeagueSlugPattern) : null,
  };

  const namesBySlug = new Map(Object.values(leagues).filter((l) => l.slug).map((l) => [l.slug, l.name]));

  for (const sport of args.sports) {
    const games = readSport(sport);
    if (sport === "soccer") {
      // Re-apply the filter to stored rows, so config changes clean up old data too.
      for (const [id, g] of games) {
        if (excluded({ slug: g.league, name: namesBySlug.get(g.league) || "" }, ctx)) games.delete(id);
      }
    }
    let from = args.from || SPORTS[sport].start;
    if (args.incremental && games.size > 0) {
      const last = [...games.values()].reduce((m, g) => (g.date > m ? g.date : m), "").slice(0, 10);
      from = addDays(last, -3);
    }
    const to = args.to || yesterday;
    const days = daysBetween(from, to);
    console.log(`${sport}: ${days.length} days (${from} to ${to}), ${games.size} games stored`);

    const before = games.size;
    let done = 0;
    await pool(days, args.concurrency, async (day) => {
      for (const row of await fetchDay(sport, day, ctx)) games.set(row.espn_id, row);
      if (++done % 60 === 0) console.log(`  ${sport}: ${done}/${days.length} days, ${games.size} games`);
    });
    const files = writeSport(sport, games);
    console.log(`${sport}: +${games.size - before} games (${games.size} total), ${files} file(s) written`);
  }

  const sortedLeagues = Object.fromEntries(Object.entries(leagues).sort(([a], [b]) => Number(a) - Number(b)));
  fs.writeFileSync(LEAGUES_FILE, JSON.stringify(sortedLeagues, null, 2) + "\n");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
