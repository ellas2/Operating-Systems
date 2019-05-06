FULL_PATH="${PATH_TO_DATA}/${DATA_FILE}"
chmod u+rwx "$FULL_PATH"
chmod g-rwx "$FULL_PATH"
chmod o-rwx "$FULL_PATH"
"${FULL_EXE_NAME}" "$FULL_PATH" "${PATTERN}" "${BOUND}" &
