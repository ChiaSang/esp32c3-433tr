#ifndef HTML_H
#define HTML_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-C3 433MHz 控制器</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #f0f2f5; padding: 12px; -webkit-text-size-adjust: 100%; }
        .container { max-width: 800px; margin: 0 auto; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                  color: white; padding: 12px; border-radius: 10px; margin-bottom: 8px;
                  display: flex; align-items: center; justify-content: space-between; }
        .header h1 { font-size: 16px; margin: 0; }
        .header p { font-size: 12px; margin-top: 2px; }
        .card { background: white; border-radius: 10px; padding: 10px 12px; margin-bottom: 8px;
                box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .card h2 { font-size: 14px; margin-bottom: 8px; }
        .btn { padding: 7px 12px; border: none; border-radius: 5px; cursor: pointer;
               font-size: 13px; transition: all 0.3s; white-space: nowrap; }
        .btn-sm { padding: 4px 6px; font-size: 12px; line-height: 1; }
        .btn-primary { background: #667eea; color: white; }
        .btn-primary:hover { background: #5568d3; }
        .btn-success { background: #48bb78; color: white; }
        .btn-success:hover { background: #38a169; }
        .btn-danger { background: #f56565; color: white; }
        .btn-danger:hover { background: #e53e3e; }
        .btn-warning { background: #ed8936; color: white; }
        .btn-warning:hover { background: #dd6b20; }
        .code-item { border: 1px solid #e2e8f0; padding: 6px 8px; margin-bottom: 4px;
                     border-radius: 6px; display: flex; align-items: center; gap: 6px; }
        .code-item.active { border-color: #667eea; background: #edf2f7; }
        .code-info { flex: 1; min-width: 0; cursor: pointer; }
        .code-name { font-weight: bold; font-size: 13px; white-space: nowrap;
                     overflow: hidden; text-overflow: ellipsis; }
        .code-details { font-size: 10px; color: #718096; line-height: 1.3;
                        white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
        .code-actions { display: flex; gap: 4px; flex-shrink: 0; }
        input[type="text"], input[type="number"] { width: 100%; padding: 10px;
                border: 1px solid #e2e8f0; border-radius: 5px; margin-bottom: 10px; }
        .status { padding: 10px; border-radius: 5px; margin-bottom: 10px; }
        .status.success { background: #c6f6d5; color: #22543d; }
        .status.error { background: #fed7d7; color: #742a2a; }
        .learning-mode { background: #fef5e7; border: 2px solid #f39c12; padding: 8px;
                        border-radius: 8px; text-align: center; margin-bottom: 8px; }
        .learning-mode h3 { font-size: 14px; }
        .learning-mode p { font-size: 12px; }
        .system-actions { display: flex; gap: 8px; flex-wrap: wrap; }
        .pagination { display: flex; justify-content: center; align-items: center; gap: 6px;
                     margin-top: 8px; padding-top: 6px; border-top: 1px solid #e2e8f0; }
        .pagination button { padding: 4px 10px; border: 1px solid #cbd5e0; border-radius: 4px;
                            background: white; cursor: pointer; font-size: 12px; }
        .pagination button:disabled { opacity: 0.4; cursor: default; }
        .pagination span { font-size: 12px; color: #718096; }
        .code-item .lp-bar { position:absolute; left:0; top:0; bottom:0; width:0; background:#f56565; border-radius:6px; opacity:0.18; pointer-events:none; transition:none; }
        .code-item.lp-active .lp-bar { width:100%; transition:width 0.8s linear; }
        .code-item .lp-hint { display:none; position:absolute; right:8px; top:50%; transform:translateY(-50%); background:#f56565; color:#fff; font-size:11px; padding:3px 8px; border-radius:4px; white-space:nowrap; pointer-events:none; z-index:2; }
        .code-item.lp-active .lp-hint { display:block; }
        .code-item { position:relative; user-select:none; -webkit-user-select:none; touch-action:manipulation; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🎛️ ESP32-C3 433MHz 控制器</h1>
            <button class="btn btn-primary" onclick="startLearning()">学习</button>
        </div>

        <div id="learningStatus" class="learning-mode" style="display:none;">
            <h3>🔴 学习模式已激活</h3>
            <p>请发射 433MHz 信号...</p>
        </div>

        <div class="card">
            <h2>📡 已保存的编码</h2>
            <div id="codeList"></div>
            <div id="pagination" class="pagination" style="display:none;">
                <button onclick="prevPage()" id="prevBtn">上一页</button>
                <span id="pageInfo">1/1</span>
                <button onclick="nextPage()" id="nextBtn">下一页</button>
            </div>
        </div>

        <div class="card">
            <h2>⚙️ 系统操作</h2>
            <div style="margin-bottom:10px; display:flex; align-items:center; gap:8px; flex-wrap:wrap;">
                <span style="font-size:13px;">发射重复次数:</span>
                <button class="btn btn-sm btn-primary" onclick="setRepeat()" id="repeatBtn">2次</button>
                <input type="range" id="repeatSlider" min="1" max="10" value="2"
                       style="width:120px; margin:0;" oninput="updateRepeatLabel()">
            </div>
            <div class="system-actions">
                <button class="btn btn-success" onclick="exportBackup()">导出备份</button>
                <button class="btn btn-warning" onclick="triggerImport()">导入备份</button>
                <input type="file" id="importFile" accept=".txt" style="display:none" onchange="importBackup(this)">
                <button class="btn btn-danger" onclick="clearAll()">清除所有编码</button>
                <button class="btn btn-primary" onclick="location.reload()">刷新页面</button>
            </div>
        </div>
        <div class="card" id="netInfo" style="margin-top:10px; font-size:12px; color:#718096;">正在获取网络信息...</div>
    </div>

    <script>
        let currentIndex = 0;
        let currentPage = 1;
        const pageSize = 5;
        let allCodes = [];

        function getEnabledCodes() { return allCodes.filter(c => c.enabled); }

        function renderPage() {
            const enabled = getEnabledCodes();
            const totalPages = Math.max(1, Math.ceil(enabled.length / pageSize));
            if (currentPage > totalPages) currentPage = totalPages;
            const start = (currentPage - 1) * pageSize;
            const page = enabled.slice(start, start + pageSize);

            const list = document.getElementById('codeList');
            if (enabled.length === 0) {
                list.innerHTML = '<p style="color:#a0aec0;">暂无保存的编码</p>';
                document.getElementById('pagination').style.display = 'none';
                return;
            }

            list.innerHTML = page.map((code) => {
                const i = code._idx;
                return `
                    <div class="code-item ${i === currentIndex ? 'active' : ''}" onclick="selectCode(${i})">
                        <div class="code-info">
                            <div class="code-name">${code.name} <small style="font-weight:normal;color:#718096;">${code.code}</small></div>
                            <div class="code-details">${code.bitlength}位 P${code.protocol} ${code.pulseLength}μs</div>
                        </div>
                        <div class="code-actions" onclick="event.stopPropagation()">
                            <button class="btn btn-sm btn-success" onclick="sendCode(${i})" title="发射">▶</button>
                            <button class="btn btn-sm btn-warning" onclick="renameCode(${i},'${code.name}')" title="命名">✏</button>
                        </div>
                        <div class="lp-bar"></div>
                        <div class="lp-hint">松开删除</div>
                    </div>`;
            }).join('');

            const pg = document.getElementById('pagination');
            pg.style.display = totalPages > 1 ? 'flex' : 'none';
            document.getElementById('pageInfo').textContent = currentPage + '/' + totalPages;
            document.getElementById('prevBtn').disabled = currentPage <= 1;
            document.getElementById('nextBtn').disabled = currentPage >= totalPages;
        }

        function prevPage() { if (currentPage > 1) { currentPage--; renderPage(); bindLongPress(); } }
        function nextPage() { const total = Math.ceil(getEnabledCodes().length / pageSize); if (currentPage < total) { currentPage++; renderPage(); bindLongPress(); } }

        async function loadCodes() {
            const res = await fetch('/api/codes');
            const data = await res.json();
            currentIndex = data.currentIndex;
            allCodes = data.codes.map((c, i) => ({...c, _idx: i}));
            renderPage();
            bindLongPress();
        }

        async function sendCode(index) {
            await fetch('/api/send/' + index);
        }

        async function selectCode(index) {
            await fetch('/api/select/' + index);
            await loadCodes();
        }

        let lpTimer = null, lpIdx = -1, lpEl = null;
        function lpStart(e) {
            const item = e.currentTarget;
            lpIdx = parseInt(item.dataset.idx);
            lpEl = item;
            item.classList.add('lp-active');
            lpTimer = setTimeout(() => {}, 850);
        }
        function lpEnd(e) {
            clearTimeout(lpTimer);
            if (lpEl && lpEl.classList.contains('lp-active')) {
                const bar = lpEl.querySelector('.lp-bar');
                const done = bar && bar.getBoundingClientRect().width >= lpEl.offsetWidth * 0.9;
                lpEl.classList.remove('lp-active');
                if (done) {
                    if (confirm('确定删除此编码？')) {
                        fetch('/api/delete/' + lpIdx).then(() => loadCodes());
                    }
                }
            }
            lpEl = null; lpIdx = -1;
        }
        function lpCancel() { clearTimeout(lpTimer); if(lpEl) lpEl.classList.remove('lp-active'); lpEl=null; lpIdx=-1; }

        function bindLongPress() {
            document.querySelectorAll('.code-item').forEach(el => {
                el.dataset.idx = el.querySelector('.code-actions .btn-success').getAttribute('onclick').match(/sendCode\((\d+)\)/)[1];
                ['mousedown','touchstart'].forEach(ev => el.addEventListener(ev, lpStart, {passive:true}));
                ['mouseup','touchend'].forEach(ev => el.addEventListener(ev, lpEnd));
                ['mouseleave','touchcancel'].forEach(ev => el.addEventListener(ev, lpCancel));
            });
        }

        async function renameCode(index, oldName) {
            const newName = prompt('请输入新名称：', oldName);
            if (newName === null || newName.trim() === '') return;
            await fetch('/api/rename/' + index + '?name=' + encodeURIComponent(newName.trim()));
            await loadCodes();
        }

        async function startLearning() {
            await fetch('/api/learn');
            document.getElementById('learningStatus').style.display = 'block';
            setTimeout(checkLearningStatus, 1000);
        }

        async function checkLearningStatus() {
            const res = await fetch('/api/status');
            const data = await res.json();
            if (data.learning) {
                setTimeout(checkLearningStatus, 1000);
            } else {
                document.getElementById('learningStatus').style.display = 'none';
                await loadCodes();
            }
        }

        async function clearAll() {
            if (!confirm('确定清除所有编码？')) return;
            await fetch('/api/clear');
            await loadCodes();
        }

        function updateRepeatLabel() {
            document.getElementById('repeatBtn').textContent = document.getElementById('repeatSlider').value + '次';
        }

        async function setRepeat() {
            const count = document.getElementById('repeatSlider').value;
            await fetch('/api/repeat/' + count);
        }

        async function exportBackup() {
            const res = await fetch('/api/backup');
            const blob = await res.blob();
            const a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = 'backup.txt';
            a.click();
            URL.revokeObjectURL(a.href);
        }

        function triggerImport() { document.getElementById('importFile').click(); }

        async function importBackup(input) {
            if (!input.files.length) return;
            const text = await input.files[0].text();
            // 统计编码行数
            const lines = text.split('\n');
            const dataLines = lines.filter(l => l.trim() && !l.startsWith('#'));
            if (dataLines.length === 0) { alert('备份文件为空'); return; }
            if (!confirm('将导入 ' + dataLines.length + ' 条编码，当前所有编码将被覆盖，确认？')) return;
            const res = await fetch('/api/restore', {method:'POST', headers:{'Content-Type':'text/plain'}, body:text});
            const msg = await res.text();
            alert(msg);
            await loadCodes();
            input.value = '';
        }

        async function loadNetwork() {
            try {
                const res = await fetch('/api/network');
                const n = await res.json();
                const rssiLevel = n.rssi > -50 ? 'excellent' : n.rssi > -60 ? 'good' : n.rssi > -70 ? 'fair' : 'weak';
                document.getElementById('netInfo').innerHTML =
                    `📶 ${n.ssid} | IP: ${n.ip}` +
                    `<br><span style="font-size:11px;opacity:0.85">` +
                    `网关: ${n.gateway} | 信号: ${n.rssi}dBm | CH: ${n.channel}` +
                    ` | MAC: ${n.mac} | 运行: ${n.uptime} | 内存: ${n.heap}B` +
                    `</span>`;
            } catch(e) {
                document.getElementById('netInfo').textContent = '网络信息获取失败';
            }
        }

        loadCodes();
        loadNetwork();
        // 同步重复次数
        fetch('/api/status').then(r=>r.json()).then(d => {
            if (d.txRepeat) { document.getElementById('repeatSlider').value = d.txRepeat; updateRepeatLabel(); }
        });
        setInterval(loadCodes, 5000);
        setInterval(loadNetwork, 60000);
    </script>
</body>
</html>
)rawliteral";

#endif // HTML_H
