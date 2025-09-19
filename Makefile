#
# JIG CLIENT APP
#
CC      = gcc
CFLAGS  = -W -Wall -g
CFLAGS  += -D__CLIENT_APP__
# CFLAGS  += -D__IPERF3_ODROID__

INCLUDE = -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lpthread
#
# 기본적으로 Makefile은 indentation가 TAB 4로 설정되어있음.
# Indentation이 space인 경우 아래 내용이 활성화 되어야 함.
# .RECIPEPREFIX +=

# 폴더이름으로 실행파일 생성
TARGET  := $(notdir $(shell pwd))

# 정의되어진 이름으로 실행파일 생성
# TARGET := test

SRC_DIRS = .
# SRCS     = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
SRCS     = $(shell find . -name "*.c")
OBJS     = $(SRCS:.c=.o)

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

TARGET_EXISTS := $(wildcard $(TARGET))

# Server MODEL config
SERVICE_DIRS = ./service
SERVICE_NAME = odroid-jig.service
SYSTEMD_DIRS = /etc/systemd/system

install :
ifndef TARGET_EXISTS
	@echo "*********************************************"
	@echo ""
	@echo "Executable file '$(TARGET)' not found!"
	@echo "Run 'make' first"
	@echo ""
	@echo "*********************************************"

else

	@echo ""
	install -m 755 $(SERVICE_DIRS)/$(SERVICE_NAME) $(SYSTEMD_DIRS)/$(SERVICE_NAME) && sync;
	@echo ""

	@if systemctl is-active --quiet $(SERVICE_NAME); then \
		echo "Reloading $(SERVICE_NAME)..."; \
		echo "" \
		systemctl restart $(SERVICE_NAME) && sync; \
	else \
		echo "$(SERVICE_NAME) not running. Enable it with:"; \
		echo "  sudo systemctl enable $(SERVICE_NAME)"; \
		echo "" \
		systemctl enable $(SERVICE_NAME) && sync; \
		systemctl restart $(SERVICE_NAME) && sync; \
	fi

	@echo ""

endif

uninstall :
	@if systemctl is-active --quiet $(SERVICE_NAME); then \
		echo "disable $(SERVICE_NAME)..."; \
		systemctl stop $(SERVICE_NAME) && sync; \
		systemctl disable $(SERVICE_NAME) && sync; \
		$(RM) $(SYSTEMD_DIRS)/$(SERVICE_NAME) && sync; \
	fi
#	$(MAKE) clean

clean :
	$(RM) $(OBJS)
	$(RM) $(TARGET)
