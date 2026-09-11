#!/bin/bash
# Прогон тестов GoidaLang. Запуск: make test  (или bash tests/run.sh)
cd "$(dirname "$0")/.." || exit 1
GOIDA=./goida
[ -x "$GOIDA" ] || { echo "сначала собери: make"; exit 1; }

pass=0; fail=0
report() { # name ok
    if [ "$2" = 0 ]; then pass=$((pass+1)); echo "PASS $1";
    else fail=$((fail+1)); echo "FAIL $1"; fi
}

for t in tests/cases/*.goida; do
    name=$(basename "$t" .goida)
    out=$(mktemp)
    "$GOIDA" "$t" >"$out" 2>&1
    code=$?
    want=0
    [ -f "tests/cases/$name.code" ] && want=$(cat "tests/cases/$name.code")
    ok=0
    if [ "$code" != "$want" ]; then
        ok=1
        echo "--- $name: код выхода $code, ждали $want"
    fi
    if [ -f "tests/cases/$name.expected" ]; then
        if ! diff -u "tests/cases/$name.expected" "$out" >"$out.diff" 2>&1; then
            ok=1
            echo "--- $name: вывод отличается:"
            cat "$out.diff"
        fi
        rm -f "$out.diff"
    fi
    if [ -f "tests/cases/$name.grep" ]; then
        while IFS= read -r line; do
            [ -z "$line" ] && continue
            if ! grep -qF "$line" "$out"; then
                ok=1
                echo "--- $name: не найдено в выводе: $line"
            fi
        done < "tests/cases/$name.grep"
    fi
    rm -f "$out"
    report "$name" $ok
done

# --- сокеты: TCP echo ---
port=9311
cat > /tmp/goida_srv_test.goida <<EOF
подключить "сокеты"
пускай srv = сокеты.Сервер("127.0.0.1", $port)
srv.слушай()
пускай c = srv.прими()
пускай s = c.получи_строку()
c.отправь_строку("эхо: " + s)
c.закрой(); srv.закрой()
EOF
cat > /tmp/goida_cli_test.goida <<EOF
подключить "сокеты"
пускай c = сокеты.Клиент("127.0.0.1", $port)
c.отправь_строку("зойда")
гойда(c.получи_строку())
c.закрой()
EOF
"$GOIDA" /tmp/goida_srv_test.goida &
srvpid=$!
sleep 0.4
cli_out=$("$GOIDA" /tmp/goida_cli_test.goida 2>&1)
wait "$srvpid" 2>/dev/null
if [ "$cli_out" = "эхо: зойда" ]; then report "sockets_tcp" 0; else report "sockets_tcp" 1; echo "--- got: $cli_out"; fi

# --- сокеты: UDP ---
udp_out=$("$GOIDA" examples/udp_эхо.goida 2>&1)
case "$udp_out" in
    *получено:*эхо:*гойда*по*udp*) report "sockets_udp" 0 ;;
    *) report "sockets_udp" 1; echo "--- got: $udp_out" ;;
esac

echo "-----------------------------"
echo "PASS=$pass FAIL=$fail"
[ "$fail" -eq 0 ]
