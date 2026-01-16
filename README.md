# OSM Wi-Fi Lab Data Prep (Overpass Turbo -> CSV -> Clean)

This mini-project pulls **building centroids** from OpenStreetMap (via **Overpass Turbo**) and converts them into a clean CSV you can use for the Wi-Fi greedy-algorithm lab.

You (or students) can run the same steps for **any** campus/city by changing the map view / bbox in Overpass Turbo.

## What this produces

Input (from Overpass Turbo): a raw CSV with lat/lon per building.

Output (from this tool):

```
id,name,lat,lon,x_m,y_m
way:12345,ASC,40.3761,-82.3967,  12.34, -56.78
...
```

Where:
- `id` is `way:...` or `relation:...`
- `x_m,y_m` are **approximate meters** in a local coordinate system (equirectangular projection) relative to the dataset’s center.

This is good enough for algorithm experiments. It is **not** a GIS-accurate projection.

---

## Step 1: Get data from Overpass Turbo

Go to: **https://overpass-turbo.eu/**

1. Pan/zoom the map to the region you want (e.g., Kenyon College).
2. Paste this query into the editor:

```overpass
[out:csv(::type,::id,name,building,::lat,::lon; true; ",")][timeout:90];
(
  way["building"]({{bbox}});
  relation["building"]({{bbox}});
);
out center;
```

3. Click **Run**.
4. Export the results:
   - Click **Export** -> choose a CSV option (e.g., “Data” / “Raw data”) and save as `buildings_raw.csv`.

Notes:
- The query uses `{{bbox}}`, which means “whatever bounding box is currently visible on the map.”
- `out center;` outputs a single center point for each building geometry.

---

## Step 2: Build the cleaner

On Linux/macOS:

```bash
make
```

This produces:

```bash
./osm_wifi_prep
```

---

## Step 3: Clean and prepare the CSV

Basic usage:

```bash
./osm_wifi_prep buildings_raw.csv buildings_clean.csv
```

Optional flags:

```bash
./osm_wifi_prep buildings_raw.csv buildings_clean.csv --dedupe --require-name
```

Flags:
- `--dedupe` : remove duplicate OSM objects (same `type:id`)
- `--require-name` : drop buildings with no `name` tag

---

## Typical workflow for a class

- Instructor provides a pre-generated dataset (e.g., Kenyon campus) for the lab.
- Students can optionally generate additional datasets for comparison (their hometown, another campus, etc.) using the same process.

---

## Suggested next step (for the Wi-Fi lab)

Once you have `buildings_clean.csv`, you can:
- treat each row as a “room/client” that must be covered
- pick a radius `R` (meters)
- ask students to place the fewest APs to cover all points under a simplified geometric model

---

## Troubleshooting

- If your CSV is empty: zoom in / adjust the bbox (some regions have sparse building tagging).
- If Overpass times out: reduce the bbox (zoom in), or try again later.
- If names are mostly missing: avoid `--require-name` (many buildings are unnamed in OSM).

