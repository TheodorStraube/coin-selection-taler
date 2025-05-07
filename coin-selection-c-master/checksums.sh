# MD5 Checksummer - Erstellt MD5-Checksummen für alle Dateien in einem Ordner
# und kann mit einer bestehenden Checksum-Datei vergleichen

# Funktion zur Anzeige der Hilfe
show_help() {
    echo "Verwendung: $0 [Optionen] <Ordnerpfad>"
    echo "Optionen:"
    echo "  -h, --help          Diese Hilfe anzeigen"
    echo "  -c, --compare FILE  Vergleiche mit bestehender Checksum-Datei"
    echo ""
    echo "Beispiel: $0 /home/user/dokumente"
    echo "Beispiel: $0 -c alte_checksums.txt /home/user/dokumente"
}

# Standardwerte
COMPARE_MODE=false
COMPARE_FILE=""
OUTPUT_FILE="md5_checksums.txt"

# Parameter verarbeiten
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--compare)
            COMPARE_MODE=true
            COMPARE_FILE="$2"
            shift 2
            ;;
        *)
            TARGET_DIR="$1"
            shift
            ;;
    esac
done

# Überprüfen, ob ein Pfad angegeben wurde
if [ -z "$TARGET_DIR" ]; then
    echo "Fehler: Kein Ordnerpfad angegeben"
    show_help
    exit 1
fi

# Überprüfen, ob der angegebene Pfad existiert und ein Verzeichnis ist
if [ ! -d "$TARGET_DIR" ]; then
    echo "Fehler: '$TARGET_DIR' ist kein gültiges Verzeichnis"
    exit 1
fi

# Überprüfen, ob im Vergleichsmodus die angegebene Datei existiert
if [ "$COMPARE_MODE" = true ] && [ ! -f "$COMPARE_FILE" ]; then
    echo "Fehler: Vergleichsdatei '$COMPARE_FILE' existiert nicht"
    exit 1
fi

echo "Erstelle MD5-Checksummen für alle Dateien in '$TARGET_DIR'..."

# Temporäre Datei für neue Checksummen
TEMP_FILE=$(mktemp)

# Dateien finden und MD5-Checksummen erstellen
find "$TARGET_DIR" -type f -exec md5sum '{}' \; > "$TEMP_FILE"

# Überprüfen, ob der Befehl erfolgreich war
if [ $? -ne 0 ]; then
    echo "Es ist ein Fehler aufgetreten beim Erstellen der MD5-Checksummen."
    rm "$TEMP_FILE"
    exit 1
fi

# Im Vergleichsmodus die Unterschiede ermitteln
if [ "$COMPARE_MODE" = true ]; then
    echo "Vergleiche mit bestehenden Checksummen aus '$COMPARE_FILE'..."
    
    # Temporäre Dateien für den Vergleich
    DIFF_FILE=$(mktemp)
    CHANGED_FILES=$(mktemp)
    
    # Sortierte Dateien für den Vergleich erstellen (nur Dateipfade)
    awk '{print $2}' "$COMPARE_FILE" | sort > "$DIFF_FILE.old"
    awk '{print $2}' "$TEMP_FILE" | sort > "$DIFF_FILE.new"
    
    # Neue oder gelöschte Dateien finden
    comm -3 "$DIFF_FILE.old" "$DIFF_FILE.new" > "$DIFF_FILE.files"
    
    # Dateien mit geänderten Checksummen finden
    while read -r file; do
        if grep -q " $file$" "$COMPARE_FILE" && grep -q " $file$" "$TEMP_FILE"; then
            old_md5=$(grep " $file$" "$COMPARE_FILE" | awk '{print $1}')
            new_md5=$(grep " $file$" "$TEMP_FILE" | awk '{print $1}')
            
            if [ "$old_md5" != "$new_md5" ]; then
                echo "$file (alt: $old_md5, neu: $new_md5)" >> "$CHANGED_FILES"
            fi
        fi
    done < <(sort -u <(awk '{print $2}' "$COMPARE_FILE") <(awk '{print $2}' "$TEMP_FILE"))
    
    # Ergebnisse ausgeben
    echo -e "\n--- Vergleichsergebnis ---"
    
    NEW_FILES=$(grep -v -f "$DIFF_FILE.old" "$DIFF_FILE.new" | wc -l)
    DELETED_FILES=$(grep -v -f "$DIFF_FILE.new" "$DIFF_FILE.old" | wc -l)
    CHANGED_COUNT=$(cat "$CHANGED_FILES" | wc -l)
    
    echo "Neue Dateien: $NEW_FILES"
    echo "Gelöschte Dateien: $DELETED_FILES"
    echo "Geänderte Dateien: $CHANGED_COUNT"
    
    if [ "$CHANGED_COUNT" -gt 0 ]; then
        echo -e "\nGeänderte Dateien (mit unterschiedlichen Checksummen):"
        cat "$CHANGED_FILES"
    fi
    
    # Aufräumen
    rm "$DIFF_FILE" "$DIFF_FILE.old" "$DIFF_FILE.new" "$DIFF_FILE.files" "$CHANGED_FILES"
    

    if [ "$CHANGED_COUNT" -gt 0 ]; then
        # Fragen, ob die neuen Checksummen gespeichert werden sollen
        echo -e "\nMöchten Sie die neuen Checksummen in '$OUTPUT_FILE' speichern? (j/n)"
        read -r answer
        if [[ "$answer" =~ ^[jJyY] ]]; then
            cp "$TEMP_FILE" "$OUTPUT_FILE"
            echo "Neue Checksummen wurden in '$OUTPUT_FILE' gespeichert."
        fi
    fi
else
    # Standardmodus: Checksummen einfach speichern
    cp "$TEMP_FILE" "$OUTPUT_FILE"
    echo "Fertig! MD5-Checksummen wurden in '$OUTPUT_FILE' gespeichert."
    echo "Gefundene Dateien: $(grep -c "" "$OUTPUT_FILE")"
fi

# Aufräumen
rm "$TEMP_FILE"
