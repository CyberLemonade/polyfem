set(SOURCES
	Form.hpp
	ALForm.hpp
	ALForm.cpp
	BodyForm.hpp
	BodyForm.cpp
	ElasticForm.hpp
	ElasticForm.cpp
	InertiaForm.hpp
	InertiaForm.cpp
	InertiaForm.hpp
	InertiaForm.cpp
	LaggedRegForm.cpp
	LaggedRegForm.hpp
	FrictionForm.cpp
	FrictionForm.hpp
	ContactForm.cpp
	ContactForm.hpp
)

prepend_current_path(SOURCES)
polyfem_set_source_group(${SOURCES})
target_sources(polyfem PRIVATE ${SOURCES})

