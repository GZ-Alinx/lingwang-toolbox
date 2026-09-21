import subprocess, sys, time, os, hashlib

GH = r'C:\Program Files\GitHub CLI\gh.exe'
REPO = 'GZ-Alinx/lingwang-toolbox'
TAG = 'v1.5.3'
WORK = r'C:\Users\itadm\AppData\Local\Temp\dmg153'

def gh(*args, **kw):
    return subprocess.run([GH, *args], capture_output=True, text=True, **kw)

os.makedirs(WORK, exist_ok=True)

# 1) 等 tag v1.5.3 的 workflow run 完成
run_id = None
for i in range(120):
    r = gh('run', 'list', '-R', REPO, '--limit', '10', '--json', 'databaseId,headBranch,status,conclusion,displayTitle')
    for line in (r.stdout or '').splitlines():
        pass
    import json
    try:
        runs = json.loads(r.stdout)
    except Exception:
        runs = []
    for run in runs:
        if run.get('headBranch') == TAG:
            run_id = run['databaseId']
            if run.get('status') == 'completed':
                print('run=%s done conclusion=%s' % (run_id, run.get('conclusion')))
                break
            print('run=%s %s' % (run_id, run.get('status')))
    if run_id:
        rr = gh('run', 'view', str(run_id), '-R', REPO, '--json', 'status,conclusion')
        try:
            j = json.loads(rr.stdout)
            if j.get('status') == 'completed':
                print('conclusion=%s' % j.get('conclusion'))
                break
        except Exception:
            pass
    time.sleep(30)
else:
    print('TIMEOUT waiting run')
    sys.exit(1)

# 2) 下载 macos-arm64 artifact
for i in range(20):
    r = gh('run', 'download', str(run_id), '-R', REPO, '-n', 'macos-arm64', '-D', WORK)
    if r.returncode == 0:
        print('artifact downloaded')
        break
    time.sleep(20)
else:
    print('download failed')
    sys.exit(1)

# 3) 找 DMG 并上传
dmg = None
for root, dirs, files in os.walk(WORK):
    for f in files:
        if f.endswith('.dmg'):
            dmg = os.path.join(root, f)
if not dmg:
    print('NO DMG in artifact')
    sys.exit(1)
print('dmg=%s (%db)' % (dmg, os.path.getsize(dmg)))
r = gh('release', 'upload', TAG, dmg, '-R', REPO, '--clobber')
print('upload rc=%d %s' % (r.returncode, (r.stderr or '')[:200]))
sys.exit(0 if r.returncode == 0 else 1)
