

import os, subprocess, tempfile
from flask import Flask, jsonify, request

EXE = ".exe" if os.name == "nt" else ""   # Windows adds .exe
ASM, EMU = os.path.abspath("asm" + EXE), os.path.abspath("emu" + EXE)
app = Flask(__name__)


PAGE = """<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>IITPSx</title>
<style>
  body { font-family: sans-serif; max-width: 700px; margin: 20px auto; padding: 0 12px; }
  textarea { width: 100%; height: 200px; font-family: monospace; }
  pre { background: #eee; color: #111; padding: 8px; overflow: auto; }
</style></head><body>
<h2>IITPSx assembler and emulator</h2>
<textarea id="source">ldc 6
ldc 7
mul
ldc 2
div
HALT</textarea>
<button onclick="run()">Assemble and run</button>
<h3>Assembler messages</h3><pre id="asm"></pre>
<h3>Emulator output</h3><pre id="emu"></pre>
<h3>Listing</h3><pre id="lst"></pre>
<script>
function run() {
  var body = JSON.stringify({source: document.getElementById("source").value});
  fetch("/run", {method: "POST", headers: {"Content-Type": "application/json"}, body: body})
    .then(r => r.json())
    .then(d => {
      for (var id of ["asm", "emu", "lst"]) document.getElementById(id).textContent = d[id];
    });
}
</script></body></html>"""


def run_command(args, folder):
    """Run a program inside folder and return (exit code, its output)."""
    p = subprocess.run(args, cwd=folder, capture_output=True, text=True, timeout=10)
    return p.returncode, p.stdout + p.stderr


@app.route("/")
def home():
    return PAGE


@app.route("/run", methods=["POST"])
def run_source():
    out = {"asm": "", "emu": "", "lst": ""}
    with tempfile.TemporaryDirectory() as folder:
        with open(os.path.join(folder, "prog.asm"), "w") as f:
            f.write(request.json["source"])
        code, out["asm"] = run_command([ASM, "prog.asm"], folder)   
        if os.path.exists(os.path.join(folder, "prog.lst")):
            out["lst"] = open(os.path.join(folder, "prog.lst")).read()
        if code == 0:                                               
            out["emu"] = run_command([EMU, "prog.o"], folder)[1]
    return jsonify(out)      #  http://127.0.0.1:5000


if __name__ == "__main__":
    app.run()
