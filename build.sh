EXE_FILE=IMTest

if [ -f ${EXE_FILE} ]; then
    rm ${EXE_FILE}
fi

gcc \
    -o ${EXE_FILE} \
    IMTest.cpp \
    -lX11

./${EXE_FILE}

