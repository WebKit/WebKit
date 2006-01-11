MODULES = JavaScriptCore WebCore WebKit

all:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; if [ $$? -ne 0 ]; then break; fi; done

debug d development dev develop:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; if [ $$? -ne 0 ]; then break; fi; done

release r deployment dep deploy:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; if [ $$? -ne 0 ]; then break; fi; done

clean:
	@for dir in $(MODULES); do ${MAKE} $@ -C $$dir; if [ $$? -ne 0 ]; then break; fi; done
