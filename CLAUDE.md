# Upute za Claude

- Korisnik piše na hrvatskom; odgovaraj na hrvatskom.
- Nakon svakog Windows builda (GitHub Actions, workflow "Windows VST3") u chatu uvijek pošalji:
  - izravni link za preuzimanje ZIP paketa (artifact `Sila-<verzija>-Windows-x64`), u obliku
    `https://github.com/voyagersquare/Silina/actions/runs/<run_id>/artifacts/<artifact_id>`
  - link na cijeli build (`https://github.com/voyagersquare/Silina/actions/runs/<run_id>`)
  - kratko: jesu li testovi i pluginval prošli.
- ID-eve dohvati s `gh api repos/voyagersquare/Silina/actions/runs/<run_id>/artifacts`.
