all:
    touch "%ConfigurationBuildDir%\buildfailed"
    bash build-generated-files.sh "%ConfigurationBuildDir%" "$(WEBKITLIBRARIESDIR)"
!IF "$(OFFICIAL_BUILD)"!="1"
    bash -c "python work-around-vs-dependency-tracking-bugs.py"
!ENDIF
    copy-files.cmd

    -del "%ConfigurationBuildDir%\buildfailed"

clean:
    -del "%ConfigurationBuildDir%\buildfailed"
    copy-files.cmd clean
