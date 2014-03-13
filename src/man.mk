EXTRA_DIST += \
	src/config.dtd.in \
	$(man1_MANS:.1=.xml) \
	$(man5_MANS:.5=.xml) \
	$(null)

CLEANFILES += \
	src/config.dtd \
	$(man1_MANS) \
	$(man5_MANS) \
	$(null)

%.1 %.5: %.xml src/config.dtd
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(XSLTPROC) \
		-o $(dir $@) \
		$(AM_XSLTPROCFLAGS) \
		--stringparam profile.condition '$(subst $(space),;,$(XSLTPROC_CONDITIONS))' \
		http://docbook.sourceforge.net/release/xsl/current/manpages/profile-docbook.xsl \
		$<

src/config.dtd: src/config.dtd.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]sysconfdir[@]:$(sysconfdir):g' \
		-e 's:[@]datadir[@]:$(datadir):g' \
		-e 's:[@]PACKAGE_NAME[@]:$(PACKAGE_NAME):g' \
		-e 's:[@]PACKAGE_VERSION[@]:$(PACKAGE_VERSION):g' \
		< $< > $@ || rm $@
